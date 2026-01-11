package com.quadracollision.blacklang;

import android.app.Activity;
import android.app.Application;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.view.View;
import android.widget.EditText;
import android.text.TextWatcher;
import android.text.Editable;
import android.view.ViewGroup;
import android.graphics.Color;
import android.view.Gravity;
import android.database.Cursor;
import android.provider.OpenableColumns;

import java.io.File;
import java.io.FileOutputStream;
import java.io.InputStream;
import java.lang.ref.WeakReference;

import com.rmsl.juce.Java;

public class BlackLangApplication extends Application {
    private static final String TAG = "BlackLangApp";
    private static final int PICK_FILE_REQUEST = 1001;

    private static WeakReference<Activity> currentActivityRef;
    private static Context appContext;

    // Native callbacks
    public static native void nativeOnFilePicked(String path);

    public static native void initFilePicker();

    public static native void nativeOnInput(int key, int charCode);

    private static EditText inputProxy;

    @Override
    public void onCreate() {
        super.onCreate();
        appContext = getApplicationContext();

        // Initialize JUCE from Java side - this ensures the correct ClassLoader is used
        Java.initialiseJUCE(getApplicationContext());

        // Initialize file picker (passes class reference to native code)
        initFilePicker();

        // Register activity lifecycle callbacks
        registerActivityLifecycleCallbacks(new ActivityLifecycleCallbacks() {
            @Override
            public void onActivityCreated(Activity activity, Bundle savedInstanceState) {
                currentActivityRef = new WeakReference<>(activity);
            }

            @Override
            public void onActivityStarted(Activity activity) {
                currentActivityRef = new WeakReference<>(activity);
            }

            @Override
            public void onActivityResumed(Activity activity) {
                currentActivityRef = new WeakReference<>(activity);
            }

            @Override
            public void onActivityPaused(Activity activity) {
            }

            @Override
            public void onActivityStopped(Activity activity) {
            }

            @Override
            public void onActivitySaveInstanceState(Activity activity, Bundle outState) {
            }

            @Override
            public void onActivityDestroyed(Activity activity) {
                if (currentActivityRef != null && currentActivityRef.get() == activity) {
                    currentActivityRef = null;
                }
            }
        });
    }

    // Called from native code to open file picker
    public static void openFilePicker(String mimeType) {
        Log.d(TAG, "openFilePicker called: " + mimeType);

        Activity activity = currentActivityRef != null ? currentActivityRef.get() : null;
        if (activity == null) {
            Log.e(TAG, "No activity available");
            nativeOnFilePicked(null);
            return;
        }

        new Handler(Looper.getMainLooper()).post(() -> {
            try {
                Intent intent = new Intent(activity, FilePickerProxyActivity.class);
                intent.putExtra(FilePickerProxyActivity.EXTRA_MIME_TYPE, mimeType);
                activity.startActivity(intent);
            } catch (Exception e) {
                Log.e(TAG, "Failed to launch proxy", e);
                nativeOnFilePicked(null);
            }
        });
    }

    // Called from native code to request permissions
    public static void requestStoragePermission() {
        Log.d(TAG, "requestStoragePermission called");
        Activity activity = currentActivityRef != null ? currentActivityRef.get() : null;
        if (activity == null)
            return;

        // For Android 13+ (API 33+), use READ_MEDIA_AUDIO
        if (android.os.Build.VERSION.SDK_INT >= 33) {
            activity.requestPermissions(new String[] {
                    "android.permission.READ_MEDIA_AUDIO"
            }, 1002);
        }
        // For Android 11+ (API 30+), need to request "All Files Access" via Settings
        else if (android.os.Build.VERSION.SDK_INT >= 30) {
            try {
                Intent intent = new Intent(android.provider.Settings.ACTION_MANAGE_ALL_FILES_ACCESS_PERMISSION);
                activity.startActivity(intent);
            } catch (Exception e) {
                Log.e(TAG, "Could not open settings", e);
            }
        }
        // For older versions, use legacy permissions
        else {
            activity.requestPermissions(new String[] {
                    android.Manifest.permission.READ_EXTERNAL_STORAGE,
                    android.Manifest.permission.WRITE_EXTERNAL_STORAGE
            }, 1002);
        }
    }

    // Called from native code to show soft keyboard
    public static void showKeyboard() {
        Log.d(TAG, "showKeyboard called");
        Activity activity = currentActivityRef != null ? currentActivityRef.get() : null;
        if (activity == null)
            return;

        new Handler(Looper.getMainLooper()).post(() -> {
            try {
                if (inputProxy == null) {
                    inputProxy = new EditText(activity);
                    // Seamless Invisible Proxy
                    // 1x1 pixel, transparent, top-left corner
                    inputProxy.setLayoutParams(new ViewGroup.LayoutParams(1, 1));
                    inputProxy.setBackgroundColor(Color.TRANSPARENT);
                    inputProxy.setTextColor(Color.TRANSPARENT);
                    inputProxy.setGravity(Gravity.TOP | Gravity.LEFT);
                    inputProxy.setCursorVisible(false);

                    // Input Type: Text, No Suggestions (to avoid auto-correct confusing the game
                    // char-stream)
                    inputProxy.setInputType(android.text.InputType.TYPE_CLASS_TEXT
                            | android.text.InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS);
                    inputProxy.setImeOptions(android.view.inputmethod.EditorInfo.IME_FLAG_NO_EXTRACT_UI);

                    inputProxy.addTextChangedListener(new TextWatcher() {
                        private boolean selfChange = false;
                        private static final char GHOST = '\u200B'; // Zero-width space

                        @Override
                        public void beforeTextChanged(CharSequence s, int start, int count, int after) {
                        }

                        @Override
                        public void onTextChanged(CharSequence s, int start, int before, int count) {
                            if (selfChange)
                                return;

                            // 1. Handle Deletions (Backspaces)
                            // "before" is the number of characters replaced/deleted
                            for (int i = 0; i < before; i++) {
                                nativeOnInput(259, 0); // Backspace
                            }

                            // 2. Handle Additions
                            // "count" is the number of new characters added
                            if (count > 0) {
                                for (int i = 0; i < count; i++) {
                                    char c = s.charAt(start + i);
                                    if (c != GHOST) { // Don't send ghost char
                                        nativeOnInput(0, (int) c);
                                    }
                                }
                            }
                        }

                        @Override
                        public void afterTextChanged(Editable s) {
                            if (selfChange)
                                return;

                            // Always keep at least the ghost char so backspace has something to delete
                            if (s.length() == 0) {
                                selfChange = true;
                                s.append(GHOST);
                                selfChange = false;
                            }
                        }
                    });

                    activity.addContentView(inputProxy, new ViewGroup.LayoutParams(1, 1));
                }

                // Initialize with ghost char so backspace works immediately
                inputProxy.setText("\u200B");
                inputProxy.setSelection(1); // Cursor at end
                inputProxy.requestFocus();

                android.view.inputmethod.InputMethodManager imm = (android.view.inputmethod.InputMethodManager) activity
                        .getSystemService(android.content.Context.INPUT_METHOD_SERVICE);
                if (imm != null) {
                    imm.showSoftInput(inputProxy, android.view.inputmethod.InputMethodManager.SHOW_FORCED);
                }
            } catch (Exception e) {
                Log.e(TAG, "Failed to show keyboard", e);
            }
        });
    }

    // Called from native code to hide soft keyboard
    public static void hideKeyboard() {
        Log.d(TAG, "hideKeyboard called");
        Activity activity = currentActivityRef != null ? currentActivityRef.get() : null;
        if (activity == null)
            return;

        new Handler(Looper.getMainLooper()).post(() -> {
            try {
                android.view.inputmethod.InputMethodManager imm = (android.view.inputmethod.InputMethodManager) activity
                        .getSystemService(android.content.Context.INPUT_METHOD_SERVICE);
                if (imm != null) {
                    if (inputProxy != null) {
                        imm.hideSoftInputFromWindow(inputProxy.getWindowToken(), 0);
                    } else if (activity.getCurrentFocus() != null) {
                        imm.hideSoftInputFromWindow(activity.getCurrentFocus().getWindowToken(), 0);
                    }
                }
            } catch (Exception e) {
                Log.e(TAG, "Failed to hide keyboard", e);
            }
        });
    }

    // Called from NativeActivity's onActivityResult (via JNI hook)
    public static void handleActivityResult(int requestCode, int resultCode, Intent data) {
        Log.d(TAG, "handleActivityResult: " + requestCode + ", " + resultCode);

        if (requestCode != PICK_FILE_REQUEST) {
            return;
        }

        if (resultCode != Activity.RESULT_OK || data == null) {
            nativeOnFilePicked(null);
            return;
        }

        Uri uri = data.getData();
        if (uri == null) {
            nativeOnFilePicked(null);
            return;
        }

        Log.d(TAG, "File URI: " + uri.toString());

        // Copy to cache for native access
        String localPath = copyFileToCache(uri);
        nativeOnFilePicked(localPath);
    }

    private static String copyFileToCache(Uri uri) {
        if (appContext == null)
            return null;

        try {
            String fileName = getFileName(uri);
            if (fileName == null) {
                fileName = "sample_" + System.currentTimeMillis() + ".wav";
            }

            File cacheDir = new File(appContext.getCacheDir(), "samples");
            if (!cacheDir.exists())
                cacheDir.mkdirs();

            File outputFile = new File(cacheDir, fileName);

            InputStream inputStream = appContext.getContentResolver().openInputStream(uri);
            if (inputStream == null)
                return null;

            FileOutputStream outputStream = new FileOutputStream(outputFile);
            byte[] buffer = new byte[8192];
            int bytesRead;
            while ((bytesRead = inputStream.read(buffer)) != -1) {
                outputStream.write(buffer, 0, bytesRead);
            }

            inputStream.close();
            outputStream.close();

            Log.d(TAG, "Copied to: " + outputFile.getAbsolutePath());
            return outputFile.getAbsolutePath();

        } catch (Exception e) {
            Log.e(TAG, "Copy failed", e);
            return null;
        }
    }

    private static String getFileName(Uri uri) {
        if (appContext == null)
            return null;
        String result = null;
        if ("content".equals(uri.getScheme())) {
            try (Cursor cursor = appContext.getContentResolver().query(uri, null, null, null, null)) {
                if (cursor != null && cursor.moveToFirst()) {
                    int index = cursor.getColumnIndex(OpenableColumns.DISPLAY_NAME);
                    if (index >= 0)
                        result = cursor.getString(index);
                }
            }
        }
        if (result == null) {
            result = uri.getPath();
            int cut = result != null ? result.lastIndexOf('/') : -1;
            if (cut != -1)
                result = result.substring(cut + 1);
        }
        return result;
    }
}
