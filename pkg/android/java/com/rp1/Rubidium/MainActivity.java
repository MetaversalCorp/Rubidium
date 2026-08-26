package com.rp1.Rubidium;

import android.graphics.Color;
import android.os.Bundle;
import android.view.Gravity;
import android.view.View;
import android.view.WindowManager;
import android.view.inputmethod.EditorInfo;
import android.widget.EditText;
import android.widget.FrameLayout;

import org.libsdl.app.SDLActivity;

public class MainActivity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] {
            "SDL3",
            "anari",
            "anari_library_halogen",
            "main"
        };
    }

    // Called from native (App_SDL.cpp) when the user submits a URL via Enter.
    // App_SDL stashes the submitted text into the active AppFrame.
    private static native void nativeUrlSubmitted (String url);

    private EditText mUrlBar;

    @Override
    protected void onCreate (Bundle savedInstanceState) {
        super.onCreate (savedInstanceState);

        mUrlBar = new EditText (this);
        mUrlBar.setSingleLine (true);
        mUrlBar.setImeOptions (EditorInfo.IME_ACTION_GO);
        mUrlBar.setBackgroundColor (Color.argb (235, 240, 240, 240));
        mUrlBar.setTextColor (Color.BLACK);
        mUrlBar.setHintTextColor (Color.GRAY);
        mUrlBar.setHint ("URL");
        int padPx = (int) (8 * getResources ().getDisplayMetrics ().density);
        mUrlBar.setPadding (padPx * 2, padPx, padPx * 2, padPx);

        int barHeightPx = (int) (44 * getResources ().getDisplayMetrics ().density);
        FrameLayout.LayoutParams lp = new FrameLayout.LayoutParams (
            FrameLayout.LayoutParams.MATCH_PARENT,
            barHeightPx,
            Gravity.TOP);
        mUrlBar.setLayoutParams (lp);

        mUrlBar.setOnEditorActionListener ((v, actionId, event) -> {
            if (actionId == EditorInfo.IME_ACTION_GO
             || actionId == EditorInfo.IME_ACTION_DONE
             || actionId == EditorInfo.IME_ACTION_NEXT) {
                nativeUrlSubmitted (v.getText ().toString ());
                v.clearFocus ();
                return true;
            }
            return false;
        });

        if (mLayout != null) {
            mLayout.addView (mUrlBar);
        }
    }

    // Called from native to seed / update the bar text from non-UI threads.
    public void setUrlText (final String url) {
        runOnUiThread (() -> {
            if (mUrlBar != null && url != null && !url.equals (mUrlBar.getText ().toString ())) {
                mUrlBar.setText (url);
            }
        });
    }
}
