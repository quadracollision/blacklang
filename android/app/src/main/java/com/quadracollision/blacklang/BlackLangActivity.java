package com.quadracollision.blacklang;

import android.app.NativeActivity;
import android.app.Activity;
import android.content.ContentResolver;
import android.content.ContentValues;
import android.content.Intent;
import android.content.Context;
import android.media.MediaScannerConnection;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.provider.MediaStore;
import android.util.Log;
import android.os.Bundle;

import java.io.File;
import java.io.FileInputStream;
import java.io.OutputStream;
import java.io.IOException;

public class BlackLangActivity extends NativeActivity {
    private static final String TAG = "BlackLangActivity";
    private static final int CREATE_FILE_REQUEST_CODE = 42;
    
    // Path of the temp file waiting to be saved
    private String pendingSourcePath = null;
    
    // Static instance for JNI access
    public static BlackLangActivity instance;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        instance = this;
        Log.d(TAG, "onCreate: BlackLangActivity started, instance set");
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        if (instance == this) {
            instance = null;
        }
    }

    // Called from C++ via JNI
    public void showToast(final String message) {
        runOnUiThread(() -> android.widget.Toast.makeText(BlackLangActivity.this, message, android.widget.Toast.LENGTH_SHORT).show());
    }

    // Called from C++ via JNI
    public void launchFileSaver(String sourcePath, String defaultName) {
        Log.d(TAG, "launchFileSaver: Source=" + sourcePath + ", DefaultName=" + defaultName);
        this.pendingSourcePath = sourcePath;
        
        runOnUiThread(() -> {
            Intent intent = new Intent(Intent.ACTION_CREATE_DOCUMENT);
            intent.addCategory(Intent.CATEGORY_OPENABLE);
            intent.setType("audio/wav");
            intent.putExtra(Intent.EXTRA_TITLE, defaultName);
            
            // Optionally set initial directory if needed (API 26+)
            // intent.putExtra(android.provider.DocumentsContract.EXTRA_INITIAL_URI, ...);

            startActivityForResult(intent, CREATE_FILE_REQUEST_CODE);
        });
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        
        if (requestCode == CREATE_FILE_REQUEST_CODE) {
            if (resultCode == Activity.RESULT_OK && data != null && data.getData() != null) {
                Uri destUri = data.getData();
                Log.d(TAG, "User selected URI: " + destUri.toString());
                
                if (pendingSourcePath != null) {
                    copyFileToUri(pendingSourcePath, destUri);
                } else {
                    Log.e(TAG, "pendingSourcePath is null!");
                    showToast("Error: source storage path lost");
                }
            } else {
                Log.d(TAG, "File Save Cancelled by user");
                showToast("Save Cancelled");
            }
            // Clear pending path
            pendingSourcePath = null;
        }
    }

    private void copyFileToUri(String sourcePath, Uri destUri) {
         new Thread(() -> {
            boolean success = false;
            try (FileInputStream in = new FileInputStream(new File(sourcePath));
                 OutputStream out = getContentResolver().openOutputStream(destUri)) {
                
                if (out != null) {
                    byte[] buffer = new byte[8192];
                    int len;
                    while ((len = in.read(buffer)) > 0) {
                        out.write(buffer, 0, len);
                    }
                    success = true;
                }
            } catch (IOException e) {
                Log.e(TAG, "Failed to copy file", e);
            }
            
            final boolean finalSuccess = success;
            runOnUiThread(() -> {
               if (finalSuccess) {
                   android.widget.Toast.makeText(this, "Saved successfully!", android.widget.Toast.LENGTH_LONG).show();
               } else {
                   android.widget.Toast.makeText(this, "Save Failed: Write Error", android.widget.Toast.LENGTH_LONG).show();
               }
            });
         }).start();
    }
}
