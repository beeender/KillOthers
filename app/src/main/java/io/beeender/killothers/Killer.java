package io.beeender.killothers;

import android.content.Context;
import android.content.SharedPreferences;

import static android.content.Context.MODE_PRIVATE;

public class Killer {

    private static final String SP_NAME_PREFIX = "KILL_OTHERS_";
    private static final String SP_VERSION_CODE = "VERSION_CODE";

    /**
     * This function try to check if the old apk's process alive while this one is running. Kill it if possible.
     *
     * @param context the {@link Context}.
     */
    public static void tryKill(Context context) {
        int versionCode = BuildConfig.VERSION_CODE;
        String myProcessName = getMyProcessName();

        SharedPreferences sharedPreferences = context.getSharedPreferences( SP_NAME_PREFIX + myProcessName,
                MODE_PRIVATE);

        if (sharedPreferences.contains(SP_VERSION_CODE) &&
                sharedPreferences.getInt(SP_VERSION_CODE, versionCode) >= versionCode) {
            // The current version apk has been started at least once before.
            return;
        }

        SharedPreferences.Editor editor = sharedPreferences.edit();
        editor.putInt(SP_VERSION_CODE, versionCode);
        editor.apply();

        // Then let's try to kill any other process with the same name and same uid.
        killOthers();
    }

    private static native void killOthers();
    private static native String getMyProcessName();
}
