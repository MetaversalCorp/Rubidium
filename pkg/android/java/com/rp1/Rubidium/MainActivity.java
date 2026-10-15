package com.rp1.Rubidium;

import android.Manifest;
import android.content.Context;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.os.Bundle;
import android.text.Editable;
import android.text.InputType;
import android.text.TextWatcher;
import android.view.Gravity;
import android.view.Surface;
import android.view.View;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.graphics.drawable.GradientDrawable;
import android.widget.Button;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ScrollView;
import android.widget.TextView;

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
    private static native void nativeMapSelected (String id);
    private static native void nativeVpsSkip ();
    private static native void nativeArSet (boolean on);
    private static native void nativeArRotation (float[] rowMajor);

    private EditText mUrlBar;
    private Button mArButton;
    private boolean mArOn;
    private FrameLayout mMapSheet;
    private boolean mImmersive;
    private boolean mSettingText;
    private boolean mAskedCamera;
    private SensorManager mSensors;
    private SensorEventListener mRotListener;
    private final float[] mRotIn = new float[9];
    private final float[] mRotOut = new float[9];

    private static final int REQ_CAMERA = 41;
    private static final String PERM_HEADSET_CAMERA = "horizonos.permission.HEADSET_CAMERA";

    // Ask once. A phone needs CAMERA. A headset also needs HEADSET_CAMERA
    // before passthrough cameras appear. A denial leaves the feed closed
    // until the user allows it from the system prompt.
    private void requestCamera () {
        if (mAskedCamera) {
            return;
        }
        boolean bCamera = checkSelfPermission (Manifest.permission.CAMERA) == PackageManager.PERMISSION_GRANTED;
        if (mImmersive) {
            boolean bHeadset = checkSelfPermission (PERM_HEADSET_CAMERA) == PackageManager.PERMISSION_GRANTED;
            if (bCamera && bHeadset) {
                return;
            }
            mAskedCamera = true;
            requestPermissions (new String[] {
                Manifest.permission.CAMERA,
                PERM_HEADSET_CAMERA
            }, REQ_CAMERA);
        } else if (!bCamera) {
            mAskedCamera = true;
            requestPermissions (new String[] {
                Manifest.permission.CAMERA
            }, REQ_CAMERA);
        }
    }

    @Override
    protected void onCreate (Bundle savedInstanceState) {
        super.onCreate (savedInstanceState);

        mImmersive = getPackageManager ().hasSystemFeature ("android.hardware.vr.headtracking");

        mUrlBar = new EditText (this);
        mUrlBar.setSingleLine (true);
        mUrlBar.setImeOptions (EditorInfo.IME_ACTION_GO);
        mUrlBar.setHint ("URL");

        if (mImmersive) {
            // 1x1 visible IME target. INVISIBLE / GONE views are skipped by
            // Horizon's overlay keyboard. Alpha 0 keeps it off the compositor.
            FrameLayout.LayoutParams lp = new FrameLayout.LayoutParams (1, 1, Gravity.TOP);
            mUrlBar.setLayoutParams (lp);
            mUrlBar.setBackgroundColor (Color.TRANSPARENT);
            mUrlBar.setTextColor (Color.TRANSPARENT);
            mUrlBar.setHintTextColor (Color.TRANSPARENT);
            mUrlBar.setInputType (InputType.TYPE_CLASS_TEXT | InputType.TYPE_TEXT_VARIATION_URI);
            mUrlBar.setFocusable (true);
            mUrlBar.setFocusableInTouchMode (true);
            mUrlBar.setAlpha (0f);
            mUrlBar.setVisibility (View.VISIBLE);
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

            mArButton = new Button (this);
            mArButton.setText ("AR");
            mArButton.setAllCaps (false);
            mArButton.setTextColor (Color.WHITE);
            GradientDrawable arBg = new GradientDrawable ();
            arBg.setColor (Color.rgb (96, 96, 96));
            arBg.setCornerRadius (8 * getResources ().getDisplayMetrics ().density);
            mArButton.setBackground (arBg);
            int arWidth = (int) (72 * getResources ().getDisplayMetrics ().density);
            mArButton.setLayoutParams (new LinearLayout.LayoutParams (arWidth, barHeightPx));
            mArButton.setOnClickListener (v -> {
                mArOn = !mArOn;
                mArButton.setText (mArOn ? "VR" : "AR");
                nativeArSet (mArOn);
            });

            LinearLayout bar = new LinearLayout (this);
            bar.setOrientation (LinearLayout.HORIZONTAL);
            LinearLayout.LayoutParams urlLp = new LinearLayout.LayoutParams (0, barHeightPx, 1f);
            mUrlBar.setLayoutParams (urlLp);
            bar.addView (mUrlBar);
            bar.addView (mArButton);
            FrameLayout.LayoutParams barLp = new FrameLayout.LayoutParams (
                FrameLayout.LayoutParams.MATCH_PARENT,
                barHeightPx,
                Gravity.TOP);
            bar.setLayoutParams (barLp);
            if (mLayout != null) {
                mLayout.addView (bar);
            }
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

        if (mImmersive && mLayout != null) {
            mLayout.addView (mUrlBar);
        }
    }

    @Override
    protected void onResume () {
        super.onResume ();
        requestCamera ();
        if (!mImmersive) {
            startRotation ();
        }
    }

    @Override
    protected void onPause () {
        stopRotation ();
        super.onPause ();
    }

    private void startRotation () {
        if (mSensors == null) {
            mSensors = (SensorManager) getSystemService (SENSOR_SERVICE);
        }
        if (mSensors == null || mRotListener != null) {
            return;
        }
        Sensor rotation = mSensors.getDefaultSensor (Sensor.TYPE_ROTATION_VECTOR);
        if (rotation == null) {
            rotation = mSensors.getDefaultSensor (Sensor.TYPE_GAME_ROTATION_VECTOR);
        }
        if (rotation == null) {
            return;
        }
        final Sensor sensor = rotation;
        mRotListener = new SensorEventListener () {
            @Override
            public void onSensorChanged (SensorEvent event) {
                if (event == null || event.values == null || event.values.length < 3) {
                    return;
                }
                SensorManager.getRotationMatrixFromVector (mRotIn, event.values);
                int nRot = Surface.ROTATION_0;
                if (getWindowManager () != null && getWindowManager ().getDefaultDisplay () != null) {
                    nRot = getWindowManager ().getDefaultDisplay ().getRotation ();
                }
                int axisX = SensorManager.AXIS_X;
                int axisY = SensorManager.AXIS_Y;
                if (nRot == Surface.ROTATION_90) {
                    axisX = SensorManager.AXIS_Y;
                    axisY = SensorManager.AXIS_MINUS_X;
                } else if (nRot == Surface.ROTATION_180) {
                    axisX = SensorManager.AXIS_MINUS_X;
                    axisY = SensorManager.AXIS_MINUS_Y;
                } else if (nRot == Surface.ROTATION_270) {
                    axisX = SensorManager.AXIS_MINUS_Y;
                    axisY = SensorManager.AXIS_X;
                }
                SensorManager.remapCoordinateSystem (mRotIn, axisX, axisY, mRotOut);
                nativeArRotation (mRotOut);
            }

            @Override
            public void onAccuracyChanged (Sensor sensor, int accuracy) {
            }
        };
        mSensors.registerListener (mRotListener, sensor, SensorManager.SENSOR_DELAY_GAME);
    }

    private void stopRotation () {
        if (mSensors != null && mRotListener != null) {
            mSensors.unregisterListener (mRotListener);
            mRotListener = null;
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

    // Quest menu / A / trigger-on-URL: focus the IME target so Horizon shows
    // the overlay keyboard.
    public void requestUrlKeyboard () {
        runOnUiThread (() -> {
            if (mUrlBar == null) {
                return;
            }
            mUrlBar.setVisibility (View.VISIBLE);
            mUrlBar.setAlpha (mImmersive ? 0f : 1f);
            mUrlBar.requestFocus ();
            mUrlBar.setSelection (mUrlBar.getText ().length ());
            InputMethodManager imm = (InputMethodManager) getSystemService (Context.INPUT_METHOD_SERVICE);
            if (imm != null) {
                imm.showSoftInput (mUrlBar, InputMethodManager.SHOW_FORCED);
                mUrlBar.postDelayed (() -> {
                    mUrlBar.requestFocus ();
                    imm.showSoftInput (mUrlBar, InputMethodManager.SHOW_FORCED);
                }, 150);
            }
        });
    }

    public void showMapPicker (final String[] labels, final String[] ids) {
        runOnUiThread (() -> {
            if (mLayout == null || mMapSheet != null || labels == null || ids == null || labels.length == 0) {
                return;
            }
            float d = getResources ().getDisplayMetrics ().density;
            mMapSheet = new FrameLayout (this);
            mMapSheet.setBackgroundColor (Color.rgb (43, 43, 43));
            mMapSheet.setClickable (true);

            LinearLayout column = new LinearLayout (this);
            column.setOrientation (LinearLayout.VERTICAL);
            int pad = (int) (16 * d);
            column.setPadding (pad, (int) (28 * d), pad, pad);

            TextView title = new TextView (this);
            title.setText ("Localization Maps");
            title.setTextColor (Color.WHITE);
            title.setGravity (Gravity.CENTER_HORIZONTAL);
            title.setTextSize (18);
            column.addView (title);

            for (int i = 0; i < labels.length && i < ids.length; i++) {
                final String id = ids[i];
                Button button = new Button (this);
                button.setText (labels[i]);
                button.setAllCaps (false);
                button.setTextColor (Color.WHITE);
                GradientDrawable bg = new GradientDrawable ();
                bg.setColor (Color.rgb (96, 96, 96));
                bg.setCornerRadius (22 * d);
                button.setBackground (bg);
                LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams (
                    LinearLayout.LayoutParams.MATCH_PARENT,
                    LinearLayout.LayoutParams.WRAP_CONTENT);
                lp.topMargin = (int) (12 * d);
                button.setLayoutParams (lp);
                button.setOnClickListener (v -> {
                    nativeMapSelected (id);
                    dismissMapPicker ();
                });
                column.addView (button);
            }

            ScrollView scroll = new ScrollView (this);
            scroll.addView (column);
            FrameLayout.LayoutParams scrollLp = new FrameLayout.LayoutParams (
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT);
            scrollLp.bottomMargin = (int) (72 * d);
            mMapSheet.addView (scroll, scrollLp);

            Button skip = new Button (this);
            skip.setText ("Skip");
            skip.setAllCaps (false);
            skip.setTextColor (Color.WHITE);
            GradientDrawable skipBg = new GradientDrawable ();
            skipBg.setColor (Color.rgb (96, 96, 96));
            skipBg.setCornerRadius (22 * d);
            skip.setBackground (skipBg);
            FrameLayout.LayoutParams skipLp = new FrameLayout.LayoutParams (
                FrameLayout.LayoutParams.WRAP_CONTENT,
                FrameLayout.LayoutParams.WRAP_CONTENT,
                Gravity.BOTTOM | Gravity.CENTER_HORIZONTAL);
            int margin = (int) (16 * d);
            skipLp.setMargins (margin, margin, margin, margin);
            skip.setOnClickListener (v -> {
                nativeVpsSkip ();
                dismissMapPicker ();
            });
            mMapSheet.addView (skip, skipLp);

            mLayout.addView (mMapSheet, new FrameLayout.LayoutParams (
                FrameLayout.LayoutParams.MATCH_PARENT,
                FrameLayout.LayoutParams.MATCH_PARENT));
        });
    }

    private void dismissMapPicker () {
        if (mMapSheet != null && mLayout != null) {
            mLayout.removeView (mMapSheet);
            mMapSheet = null;
        }
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
