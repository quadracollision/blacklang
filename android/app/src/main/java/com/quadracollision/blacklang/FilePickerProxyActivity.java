package com.quadracollision.blacklang;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;
import android.database.Cursor;
import android.provider.OpenableColumns;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;

/**
 * Transparent activity for handling file picking.
 * This exists because NativeActivity doesn't properly handle onActivityResult.
 */
public class FilePickerProxyActivity extends Activity {
    private static final String TAG = "FilePickerProxy";
    private static final int PICK_FILE_REQUEST = 1001;

    public static final String EXTRA_MIME_TYPE = "mime_type";

    // Removed local native declaration

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        String mimeType = getIntent().getStringExtra(EXTRA_MIME_TYPE);
        if (mimeType == null)
            mimeType = "audio/*";

        Log.d(TAG, "Launching file picker for: " + mimeType);

        Intent intent = new Intent(Intent.ACTION_OPEN_DOCUMENT);
        intent.addCategory(Intent.CATEGORY_OPENABLE);
        intent.setType(mimeType);

        if (mimeType.equals("audio/*")) {
            intent.putExtra(Intent.EXTRA_MIME_TYPES, new String[] {
                    "audio/wav", "audio/wave", "audio/x-wav",
                    "audio/mpeg", "audio/ogg", "audio/*"
            });
        }

        startActivityForResult(intent, PICK_FILE_REQUEST);
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);

        Log.d(TAG, "onActivityResult: " + requestCode + ", " + resultCode);

        if (requestCode != PICK_FILE_REQUEST) {
            finish();
            return;
        }

        if (resultCode != RESULT_OK || data == null) {
            Log.d(TAG, "Cancelled");
            BlackLangApplication.nativeOnFilePicked(null);
            finish();
            return;
        }

        Uri uri = data.getData();
        if (uri == null) {
            BlackLangApplication.nativeOnFilePicked(null);
            finish();
            return;
        }

        Log.d(TAG, "Selected: " + uri);
        String localPath = copyToCache(uri);
        BlackLangApplication.nativeOnFilePicked(localPath);
        finish();
    }

    private String copyToCache(Uri uri) {
        try {
            String fileName = getFileName(uri);
            if (fileName == null)
                fileName = "sample_" + System.currentTimeMillis() + ".wav";

            File cacheDir = new File(getCacheDir(), "samples");
            if (!cacheDir.exists())
                cacheDir.mkdirs();

            File outFile = new File(cacheDir, fileName);

            InputStream in = getContentResolver().openInputStream(uri);
            if (in == null)
                return null;

            FileOutputStream out = new FileOutputStream(outFile);
            byte[] buf = new byte[8192];
            int len;
            while ((len = in.read(buf)) > 0)
                out.write(buf, 0, len);
            in.close();
            out.close();

            Log.d(TAG, "Copied to: " + outFile.getAbsolutePath());
            return outFile.getAbsolutePath();
        } catch (Exception e) {
            Log.e(TAG, "Copy failed", e);
            return null;
        }
    }

    private String getFileName(Uri uri) {
        String result = null;
        if ("content".equals(uri.getScheme())) {
            try (Cursor c = getContentResolver().query(uri, null, null, null, null)) {
                if (c != null && c.moveToFirst()) {
                    int i = c.getColumnIndex(OpenableColumns.DISPLAY_NAME);
                    if (i >= 0)
                        result = c.getString(i);
                }
            }
        }
        if (result == null) {
            String path = uri.getPath();
            if (path != null) {
                int cut = path.lastIndexOf('/');
                if (cut >= 0)
                    result = path.substring(cut + 1);
            }
        }
        return result;
    }
}
