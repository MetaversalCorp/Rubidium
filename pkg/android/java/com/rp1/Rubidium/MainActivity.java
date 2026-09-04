package com.rp1.Rubidium;

import android.Manifest;
import android.content.Context;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.graphics.Matrix;
import android.graphics.PixelFormat;
import android.graphics.RectF;
import android.graphics.SurfaceTexture;
import android.graphics.drawable.ColorDrawable;
import android.hardware.camera2.CameraAccessException;
import android.hardware.camera2.CameraCaptureSession;
import android.hardware.camera2.CameraCharacteristics;
import android.hardware.camera2.CameraDevice;
import android.hardware.camera2.CameraManager;
import android.hardware.camera2.CaptureRequest;
import android.hardware.camera2.params.StreamConfigurationMap;
import android.os.Bundle;
import android.os.Handler;
import android.os.HandlerThread;
import android.util.Log;
import android.util.Size;
import android.view.Gravity;
import android.view.Surface;
import android.view.TextureView;
import android.view.View;
import android.view.WindowManager;
import android.view.inputmethod.EditorInfo;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;

import org.libsdl.app.SDLActivity;
import org.libsdl.app.SDLSurface;

import java.util.Collections;

public class MainActivity extends SDLActivity {
    private static final String TAG = "Rubidium";
    private static final int REQUEST_CAMERA = 0x5201;

    @Override
    protected String[] getLibraries() {
        return new String[] {
            "SDL3",
            "anari",
            "anari_library_halogen",
            "main"
        };
    }

    private static native void nativeUrlSubmitted (String url);
    private static native void nativePassthrough (boolean enable);

    private EditText mUrlBar;
    private Button mModeButton;
    private LinearLayout mChrome;
    private int mChromeHeightPx;
    private boolean mChromeAttached;
    private TextureView mCameraView;
    private boolean mArMode;

    private final Object mCameraLock = new Object ();
    private HandlerThread mCameraThread;
    private Handler mCameraHandler;
    private CameraDevice mCamera;
    private CameraCaptureSession mCaptureSession;
    private Surface mPreviewSurface;
    private Size mPreviewSize;
    private int mSensorOrientation = 90;
    private boolean mCameraOpening;

    private final TextureView.SurfaceTextureListener mCameraTextureListener =
        new TextureView.SurfaceTextureListener () {
            @Override
            public void onSurfaceTextureAvailable (SurfaceTexture surface, int width, int height) {
                applyPreviewTransform ();
                if (mArMode)
                    startCamera ();
            }

            @Override
            public void onSurfaceTextureSizeChanged (SurfaceTexture surface, int width, int height) {
                applyPreviewTransform ();
            }

            @Override
            public boolean onSurfaceTextureDestroyed (SurfaceTexture surface) {
                stopCamera ();
                return true;
            }

            @Override
            public void onSurfaceTextureUpdated (SurfaceTexture surface) {
            }
        };

    private final CameraDevice.StateCallback mCameraStateCallback = new CameraDevice.StateCallback () {
        @Override
        public void onOpened (CameraDevice camera) {
            Log.i (TAG, "camera opened");
            synchronized (mCameraLock) {
                mCamera = camera;
                mCameraOpening = false;
            }
            createPreviewSession (camera);
        }

        @Override
        public void onDisconnected (CameraDevice camera) {
            camera.close ();
            synchronized (mCameraLock) {
                mCameraOpening = false;
                if (mCamera == camera)
                    mCamera = null;
            }
        }

        @Override
        public void onError (CameraDevice camera, int error) {
            Log.e (TAG, "CameraDevice error " + error);
            camera.close ();
            synchronized (mCameraLock) {
                mCameraOpening = false;
                if (mCamera == camera)
                    mCamera = null;
            }
            runOnUiThread (new Runnable () {
                @Override
                public void run () {
                    ToastMsg ("Camera error");
                    setArMode (false);
                }
            });
        }
    };

    @Override
    protected SDLSurface createSDLSurface (Context context) {
        SDLSurface surface = super.createSDLSurface (context);
        // Filament's transparent SurfaceView recipe: TRANSLUCENT + z-order on
        // top so the 3D layer composites over window content (the camera
        // TextureView). Must be set before the view is attached.
        surface.getHolder ().setFormat (PixelFormat.TRANSLUCENT);
        surface.setZOrderOnTop (true);
        return surface;
    }

    @Override
    protected void onCreate (Bundle savedInstanceState) {
        getWindow ().setFormat (PixelFormat.TRANSLUCENT);
        getWindow ().setBackgroundDrawable (new ColorDrawable (Color.BLACK));
        super.onCreate (savedInstanceState);

        if (mLayout == null)
            return;

        mLayout.setBackgroundColor (Color.BLACK);

        mCameraView = new TextureView (this);
        mCameraView.setOpaque (true);
        mCameraView.setSurfaceTextureListener (mCameraTextureListener);
        mLayout.addView (mCameraView, 0, new android.widget.RelativeLayout.LayoutParams (
            android.widget.RelativeLayout.LayoutParams.MATCH_PARENT,
            android.widget.RelativeLayout.LayoutParams.MATCH_PARENT));
        mCameraView.setVisibility (View.GONE);

        float density = getResources ().getDisplayMetrics ().density;
        int padPx = (int) (8 * density);
        mChromeHeightPx = (int) (44 * density);

        mChrome = new LinearLayout (this);
        mChrome.setOrientation (LinearLayout.HORIZONTAL);
        mChrome.setBackgroundColor (Color.argb (235, 240, 240, 240));
        mChrome.setGravity (Gravity.CENTER_VERTICAL);

        mUrlBar = new EditText (this);
        mUrlBar.setSingleLine (true);
        mUrlBar.setImeOptions (EditorInfo.IME_ACTION_GO);
        mUrlBar.setBackgroundColor (Color.TRANSPARENT);
        mUrlBar.setTextColor (Color.BLACK);
        mUrlBar.setHintTextColor (Color.GRAY);
        mUrlBar.setHint ("URL");
        mUrlBar.setPadding (padPx * 2, padPx, padPx, padPx);
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

        mModeButton = new Button (this);
        mModeButton.setText ("VR");
        mModeButton.setAllCaps (false);
        mModeButton.setTextColor (Color.BLACK);
        mModeButton.setPadding (padPx * 2, 0, padPx * 2, 0);
        mModeButton.setOnClickListener (new View.OnClickListener () {
            @Override
            public void onClick (View v) {
                setArMode (!mArMode);
            }
        });

        LinearLayout.LayoutParams urlLp = new LinearLayout.LayoutParams (
            0, LinearLayout.LayoutParams.MATCH_PARENT, 1.0f);
        LinearLayout.LayoutParams btnLp = new LinearLayout.LayoutParams (
            LinearLayout.LayoutParams.WRAP_CONTENT,
            LinearLayout.LayoutParams.MATCH_PARENT);
        mChrome.addView (mUrlBar, urlLp);
        mChrome.addView (mModeButton, btnLp);

        attachChrome ();
    }

    @Override
    protected void onPause () {
        if (mArMode)
            stopCamera ();
        super.onPause ();
    }

    @Override
    protected void onResume () {
        super.onResume ();
        if (mArMode)
            startCamera ();
    }

    @Override
    protected void onDestroy () {
        stopCamera ();
        quitCameraThread ();
        detachChrome ();
        super.onDestroy ();
    }

    @Override
    public void onRequestPermissionsResult (int requestCode, String[] permissions, int[] grantResults) {
        if (requestCode == REQUEST_CAMERA) {
            if (grantResults.length > 0 && grantResults[0] == PackageManager.PERMISSION_GRANTED)
                setArMode (true);
            else
                ToastMsg ("Camera permission is required for AR");
            return;
        }
        super.onRequestPermissionsResult (requestCode, permissions, grantResults);
    }

    public void setUrlText (final String url) {
        runOnUiThread (() -> {
            if (mUrlBar != null && url != null && !url.equals (mUrlBar.getText ().toString ())) {
                mUrlBar.setText (url);
            }
        });
    }

    private void attachChrome () {
        if (mChrome == null || mChromeAttached)
            return;

        View decor = getWindow ().getDecorView ();
        if (decor.getWindowToken () == null) {
            decor.post (new Runnable () {
                @Override
                public void run () {
                    attachChrome ();
                }
            });
            return;
        }

        WindowManager.LayoutParams lp = new WindowManager.LayoutParams (
            WindowManager.LayoutParams.MATCH_PARENT,
            mChromeHeightPx,
            WindowManager.LayoutParams.TYPE_APPLICATION_SUB_PANEL,
            WindowManager.LayoutParams.FLAG_NOT_TOUCH_MODAL
                | WindowManager.LayoutParams.FLAG_LAYOUT_IN_SCREEN,
            PixelFormat.TRANSLUCENT);
        lp.gravity = Gravity.TOP;
        lp.token = decor.getWindowToken ();
        try {
            getWindowManager ().addView (mChrome, lp);
            mChromeAttached = true;
        } catch (Exception e) {
            Log.w (TAG, "SUB_PANEL chrome failed, falling back to APPLICATION_PANEL", e);
            lp.type = WindowManager.LayoutParams.TYPE_APPLICATION_PANEL;
            getWindowManager ().addView (mChrome, lp);
            mChromeAttached = true;
        }
    }

    private void detachChrome () {
        if (mChromeAttached && mChrome != null) {
            try {
                getWindowManager ().removeView (mChrome);
            } catch (Exception ignored) {
            }
            mChromeAttached = false;
        }
    }

    private void setArMode (boolean enable) {
        if (enable == mArMode)
            return;

        if (enable) {
            if (checkSelfPermission (Manifest.permission.CAMERA) != PackageManager.PERMISSION_GRANTED) {
                requestPermissions (new String[] { Manifest.permission.CAMERA }, REQUEST_CAMERA);
                return;
            }

            mArMode = true;
            if (mModeButton != null)
                mModeButton.setText ("AR");
            mLayout.setBackgroundColor (Color.TRANSPARENT);
            nativePassthrough (true);
            if (mCameraView != null) {
                mCameraView.setVisibility (View.VISIBLE);
                if (mCameraView.isAvailable ())
                    startCamera ();
            }
            Log.i (TAG, "AR passthrough enabled");
        } else {
            mArMode = false;
            if (mModeButton != null)
                mModeButton.setText ("VR");
            stopCamera ();
            if (mCameraView != null)
                mCameraView.setVisibility (View.GONE);
            mLayout.setBackgroundColor (Color.BLACK);
            nativePassthrough (false);
            Log.i (TAG, "AR passthrough disabled");
        }
    }

    private void ensureCameraThread () {
        if (mCameraThread == null) {
            mCameraThread = new HandlerThread ("RubidiumCamera");
            mCameraThread.start ();
            mCameraHandler = new Handler (mCameraThread.getLooper ());
        }
    }

    private void quitCameraThread () {
        if (mCameraThread != null) {
            mCameraThread.quitSafely ();
            mCameraThread = null;
            mCameraHandler = null;
        }
    }

    private void startCamera () {
        if (!mArMode || mCameraView == null)
            return;

        synchronized (mCameraLock) {
            if (mCamera != null || mCameraOpening)
                return;
            mCameraOpening = true;
        }

        SurfaceTexture texture = mCameraView.getSurfaceTexture ();
        if (texture == null) {
            synchronized (mCameraLock) {
                mCameraOpening = false;
            }
            return;
        }

        if (checkSelfPermission (Manifest.permission.CAMERA) != PackageManager.PERMISSION_GRANTED) {
            synchronized (mCameraLock) {
                mCameraOpening = false;
            }
            return;
        }

        ensureCameraThread ();

        CameraManager manager = (CameraManager) getSystemService (Context.CAMERA_SERVICE);
        if (manager == null) {
            synchronized (mCameraLock) {
                mCameraOpening = false;
            }
            return;
        }

        try {
            String cameraId = chooseCameraId (manager);
            if (cameraId == null) {
                synchronized (mCameraLock) {
                    mCameraOpening = false;
                }
                ToastMsg ("No camera available");
                setArMode (false);
                return;
            }

            CameraCharacteristics chars = manager.getCameraCharacteristics (cameraId);
            Integer sensor = chars.get (CameraCharacteristics.SENSOR_ORIENTATION);
            mSensorOrientation = (sensor != null) ? sensor.intValue () : 90;
            Size preview = choosePreviewSize (chars, mCameraView.getWidth (), mCameraView.getHeight ());
            mPreviewSize = preview;
            texture.setDefaultBufferSize (preview.getWidth (), preview.getHeight ());
            applyPreviewTransform ();
            Log.i (TAG, "opening camera " + cameraId + " preview " + preview.getWidth () + "x" + preview.getHeight ()
                + " sensor " + mSensorOrientation);
            manager.openCamera (cameraId, mCameraStateCallback, mCameraHandler);
        } catch (CameraAccessException | SecurityException e) {
            Log.e (TAG, "openCamera failed", e);
            synchronized (mCameraLock) {
                mCameraOpening = false;
            }
            ToastMsg ("Could not open camera");
            setArMode (false);
        }
    }

    private void createPreviewSession (CameraDevice camera) {
        if (mCameraView == null)
            return;

        SurfaceTexture texture = mCameraView.getSurfaceTexture ();
        if (texture == null) {
            camera.close ();
            return;
        }

        try {
            Surface surface = new Surface (texture);
            synchronized (mCameraLock) {
                if (mPreviewSurface != null)
                    mPreviewSurface.release ();
                mPreviewSurface = surface;
            }
            final CaptureRequest.Builder builder = camera.createCaptureRequest (CameraDevice.TEMPLATE_PREVIEW);
            builder.addTarget (surface);
            camera.createCaptureSession (Collections.singletonList (surface),
                new CameraCaptureSession.StateCallback () {
                    @Override
                    public void onConfigured (CameraCaptureSession session) {
                        Log.i (TAG, "camera preview session started");
                        synchronized (mCameraLock) {
                            mCaptureSession = session;
                            try {
                                session.setRepeatingRequest (builder.build (), null, mCameraHandler);
                            } catch (CameraAccessException e) {
                                Log.e (TAG, "setRepeatingRequest failed", e);
                            }
                        }
                        runOnUiThread (new Runnable () {
                            @Override
                            public void run () {
                                applyPreviewTransform ();
                            }
                        });
                    }

                    @Override
                    public void onConfigureFailed (CameraCaptureSession session) {
                        Log.e (TAG, "CameraCaptureSession configure failed");
                        runOnUiThread (new Runnable () {
                            @Override
                            public void run () {
                                ToastMsg ("Camera preview failed");
                                setArMode (false);
                            }
                        });
                    }
                }, mCameraHandler);
        } catch (CameraAccessException e) {
            Log.e (TAG, "createCaptureSession failed", e);
        }
    }

    private void stopCamera () {
        synchronized (mCameraLock) {
            if (mCaptureSession != null) {
                try {
                    mCaptureSession.close ();
                } catch (Exception ignored) {
                }
                mCaptureSession = null;
            }
            if (mCamera != null) {
                try {
                    mCamera.close ();
                } catch (Exception ignored) {
                }
                mCamera = null;
            }
            if (mPreviewSurface != null) {
                mPreviewSurface.release ();
                mPreviewSurface = null;
            }
            mPreviewSize = null;
            mCameraOpening = false;
        }
    }

    // Camera2 buffers stay in sensor space. TextureView then stretches that
    // rectangle to the view (rotated 90 deg on a landscape activity, and
    // non-uniform). This is the Camera2Basic transform: swap buffer axes,
    // uniform scale to fill, rotate to the display.
    private void applyPreviewTransform ()
    {
        if (mCameraView == null || mPreviewSize == null)
            return;

        int nViewW = mCameraView.getWidth ();
        int nViewH = mCameraView.getHeight ();
        if (nViewW == 0 || nViewH == 0)
            return;

        int nRotation = (android.os.Build.VERSION.SDK_INT >= 30)
            ? getDisplay ().getRotation ()
            : getWindowManager ().getDefaultDisplay ().getRotation ();

        Matrix matrix = new Matrix ();
        RectF viewRect = new RectF (0, 0, nViewW, nViewH);
        RectF bufferRect = new RectF (0, 0, mPreviewSize.getHeight (), mPreviewSize.getWidth ());
        float cx = viewRect.centerX ();
        float cy = viewRect.centerY ();

        if (nRotation == Surface.ROTATION_90 || nRotation == Surface.ROTATION_270)
        {
            bufferRect.offset (cx - bufferRect.centerX (), cy - bufferRect.centerY ());
            matrix.setRectToRect (viewRect, bufferRect, Matrix.ScaleToFit.FILL);
            float fScale = Math.max (
                (float) nViewH / mPreviewSize.getHeight (),
                (float) nViewW / mPreviewSize.getWidth ());
            matrix.postScale (fScale, fScale, cx, cy);
            matrix.postRotate (90 * (nRotation - 2), cx, cy);
        }
        else if (nRotation == Surface.ROTATION_180)
        {
            matrix.postRotate (180, cx, cy);
        }

        mCameraView.setTransform (matrix);
        Log.i (TAG, "preview transform displayRot=" + nRotation
            + " view=" + nViewW + "x" + nViewH
            + " buffer=" + mPreviewSize.getWidth () + "x" + mPreviewSize.getHeight ());
    }

    private static String chooseCameraId (CameraManager manager) throws CameraAccessException {
        String fallback = null;
        String[] ids = manager.getCameraIdList ();
        for (int i = 0; i < ids.length; i++) {
            String id = ids[i];
            CameraCharacteristics chars = manager.getCameraCharacteristics (id);
            Integer facing = chars.get (CameraCharacteristics.LENS_FACING);
            if (facing != null && facing.intValue () == CameraCharacteristics.LENS_FACING_BACK)
                return id;
            if (fallback == null)
                fallback = id;
        }
        return fallback;
    }

    private static Size choosePreviewSize (CameraCharacteristics chars, int nWidth, int nHeight) {
        StreamConfigurationMap map = chars.get (CameraCharacteristics.SCALER_STREAM_CONFIGURATION_MAP);
        Size fallback = new Size (Math.max (nWidth, 1280), Math.max (nHeight, 720));
        if (map == null)
            return fallback;

        Size[] sizes = map.getOutputSizes (SurfaceTexture.class);
        if (sizes == null || sizes.length == 0)
            return fallback;

        int nTarget = Math.max (nWidth, 1) * Math.max (nHeight, 1);
        Size best = sizes[0];
        int nBestDiff = Integer.MAX_VALUE;
        for (int i = 0; i < sizes.length; i++) {
            Size size = sizes[i];
            int nDiff = Math.abs (size.getWidth () * size.getHeight () - nTarget);
            if (nDiff < nBestDiff) {
                nBestDiff = nDiff;
                best = size;
            }
        }
        return best;
    }

    private void ToastMsg (String sMessage) {
        android.widget.Toast.makeText (this, sMessage, android.widget.Toast.LENGTH_SHORT).show ();
    }
}
