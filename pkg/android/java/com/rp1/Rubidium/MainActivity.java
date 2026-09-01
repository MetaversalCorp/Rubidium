package com.rp1.Rubidium;

import android.content.Context;
import android.graphics.Color;
import android.os.Bundle;
import android.text.Editable;
import android.text.TextWatcher;
import android.view.Gravity;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
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
    private static native void nativeUrlTextChanged (String url);

    private EditText mUrlBar;
    private boolean mImmersive;
    private boolean mSettingText;

    @Override
    protected void onCreate (Bundle savedInstanceState) {
        super.onCreate (savedInstanceState);

        mImmersive = getPackageManager ().hasSystemFeature ("android.hardware.vr.headtracking");

        mUrlBar = new EditText (this);
        mUrlBar.setSingleLine (true);
        mUrlBar.setImeOptions (EditorInfo.IME_ACTION_GO);
        mUrlBar.setHint ("URL");

        if (mImmersive) {
            // Hidden IME focus target. Horizon's system keyboard overlay is the
            // visible typing UI in the HMD; this view is never composited there.
            FrameLayout.LayoutParams lp = new FrameLayout.LayoutParams (1, 1, Gravity.TOP);
            mUrlBar.setLayoutParams (lp);
            mUrlBar.setBackgroundColor (Color.TRANSPARENT);
            mUrlBar.setTextColor (Color.TRANSPARENT);
            mUrlBar.setHintTextColor (Color.TRANSPARENT);
            mUrlBar.setVisibility (View.INVISIBLE);
        } else {
            mUrlBar.setBackgroundColor (Color.argb (235, 240, 240, 240));
            mUrlBar.setTextColor (Color.BLACK);
            mUrlBar.setHintTextColor (Color.GRAY);
            int padPx = (int) (8 * getResources ().getDisplayMetrics ().density);
            mUrlBar.setPadding (padPx * 2, padPx, padPx * 2, padPx);
            int barHeightPx = (int) (44 * getResources ().getDisplayMetrics ().density);
            FrameLayout.LayoutParams lp = new FrameLayout.LayoutParams (
                FrameLayout.LayoutParams.MATCH_PARENT,
                barHeightPx,
                Gravity.TOP);
            mUrlBar.setLayoutParams (lp);
        }

        mUrlBar.setOnEditorActionListener ((v, actionId, event) -> {
            if (actionId == EditorInfo.IME_ACTION_GO
             || actionId == EditorInfo.IME_ACTION_DONE
             || actionId == EditorInfo.IME_ACTION_NEXT) {
                nativeUrlSubmitted (v.getText ().toString ());
                v.clearFocus ();
                hideUrlKeyboard ();
                return true;
            }
            return false;
        });

        mUrlBar.addTextChangedListener (new TextWatcher () {
            @Override public void beforeTextChanged (CharSequence s, int start, int count, int after) {}
            @Override public void onTextChanged (CharSequence s, int start, int before, int count) {}
            @Override public void afterTextChanged (Editable s) {
                if (!mSettingText && s != null) {
                    nativeUrlTextChanged (s.toString ());
                }
            }
        });

        if (mLayout != null) {
            mLayout.addView (mUrlBar);
        }
    }

    // Called from native to seed / update the bar text from non-UI threads.
    public void setUrlText (final String url) {
        runOnUiThread (() -> {
            if (mUrlBar != null && url != null && !url.equals (mUrlBar.getText ().toString ())) {
                mSettingText = true;
                mUrlBar.setText (url);
                mSettingText = false;
            }
        });
    }

    // Quest menu / A (and desktop fallback): focus the hidden EditText so Horizon
    // shows the system keyboard overlay.
    public void requestUrlKeyboard () {
        runOnUiThread (() -> {
            if (mUrlBar == null) {
                return;
            }
            mUrlBar.setVisibility (mImmersive ? View.INVISIBLE : View.VISIBLE);
            mUrlBar.requestFocus ();
            InputMethodManager imm = (InputMethodManager) getSystemService (Context.INPUT_METHOD_SERVICE);
            if (imm != null) {
                imm.showSoftInput (mUrlBar, InputMethodManager.SHOW_FORCED);
            }
        });
    }

    public void hideUrlKeyboard () {
        runOnUiThread (() -> {
            if (mUrlBar == null) {
                return;
            }
            InputMethodManager imm = (InputMethodManager) getSystemService (Context.INPUT_METHOD_SERVICE);
            if (imm != null) {
                imm.hideSoftInputFromWindow (mUrlBar.getWindowToken (), 0);
            }
            mUrlBar.clearFocus ();
        });
    }
}
