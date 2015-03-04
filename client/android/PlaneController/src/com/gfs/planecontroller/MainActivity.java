package com.gfs.planecontroller;

import com.client.nativef.ClientNative;

import android.app.Activity;
import android.content.Context;
import android.hardware.Sensor;
import android.hardware.SensorEvent;
import android.hardware.SensorEventListener;
import android.hardware.SensorManager;
import android.os.Bundle;
import android.util.Log;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.SeekBar;
import android.widget.Toast;
import android.widget.SeekBar.OnSeekBarChangeListener;

public class MainActivity extends Activity implements SensorEventListener, OnSeekBarChangeListener {
	private SensorManager mSensorMgr;
	private Sensor mSensor, mOrientationSensor;

	private OrientationView mOrientationView;
	private SeekBar mThrottleView;
	private SeekBar mAltitudeView;
	private Button mConnectBtn;
	private Button mSelftestBtn;
	private EditText mHostEditor;
	
	private int mLastRoll = 0, mLastPitch = 0, mLastYaw = 0;
	private Boolean mConnected = false;

	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.activity_main);
		
		mOrientationView = (OrientationView)findViewById(R.id.planeview);
		mThrottleView = (SeekBar)findViewById(R.id.throttleview);
		mThrottleView.setOnSeekBarChangeListener(this);
		mAltitudeView = (SeekBar)findViewById(R.id.altitudeview);
		mAltitudeView.setOnSeekBarChangeListener(this);
		mConnectBtn = (Button)findViewById(R.id.btn_conn);
		mSelftestBtn = (Button)findViewById(R.id.btn_selftest);
		mHostEditor = (EditText)findViewById(R.id.addrview);
	    mHostEditor.setText("172.16.20.1");

		mConnectBtn.setOnClickListener(new View.OnClickListener() {

			@Override
			public void onClick(View arg0) {
				String host = mHostEditor.getEditableText().toString();
				int err = ClientNative.connect(host, 8888);
				if (0 == err) {
					mConnected = true;
					Toast.makeText(getApplicationContext(), "Connect success", Toast.LENGTH_SHORT).show();
				} else {
					Toast.makeText(getApplicationContext(), "Connect Error " + err, Toast.LENGTH_SHORT).show();
				}
			}
		});
		
		mSelftestBtn.setOnClickListener(new View.OnClickListener() {

			@Override
			public void onClick(View arg0) {
				if (!mConnected) {
					return;
				}

				int err = ClientNative.sendcmd("euler --self-test;\r\n");
				if (0 == err) {
					Toast.makeText(getApplicationContext(), "Self-test success", Toast.LENGTH_SHORT).show();
				} else {
					Toast.makeText(getApplicationContext(), "Self-test Error " + err, Toast.LENGTH_SHORT).show();
				}
			}
		});

		initSensor();
	}

	@SuppressWarnings("deprecation")
	private void initSensor() {
		if(null == mSensorMgr) {
			mSensorMgr = (SensorManager)getSystemService(Context.SENSOR_SERVICE);
		}
		mSensor = mSensorMgr.getDefaultSensor(Sensor.TYPE_ACCELEROMETER);
		mSensorMgr.registerListener(this, mSensor, SensorManager.SENSOR_DELAY_NORMAL);
		
		mOrientationSensor = mSensorMgr.getDefaultSensor(Sensor.TYPE_ORIENTATION);
		mSensorMgr.registerListener(this, mOrientationSensor, SensorManager.SENSOR_DELAY_NORMAL);
	}
	
	@Override
	protected void onDestroy() {
		super.onDestroy();
		mSensorMgr.unregisterListener(this);
	}
	
	
	@Override
	public boolean onCreateOptionsMenu(Menu menu) {
		// Inflate the menu; this adds items to the action bar if it is present.
		return true;
	}

	@Override
	public boolean onOptionsItemSelected(MenuItem item) {
		return super.onOptionsItemSelected(item);
	}

	@SuppressWarnings("deprecation")
	@Override
	public void onSensorChanged(SensorEvent event) {
		if (event.sensor == null) {
			return;
		}

		if (event.sensor.getType() == Sensor.TYPE_ACCELEROMETER) {
			
			int Roll = 0, Pitch = 0, Yaw = 0;

			int x = (int) event.values[0];
			int y = (int) event.values[1];
			int z = (int) event.values[2];
			
			if (!mConnected) {
				return;
			}
			
			Roll = (int)(Math.atan2(x, z) * (180 / 3.14));
			Pitch = (int)(Math.atan2(y, z) * (180 / 3.14));
			Yaw = 0;

			if (Roll == mLastRoll && Pitch == mLastPitch && Yaw == mLastYaw) {
				return;
			}

			mLastRoll = Roll;
			mLastPitch = Pitch;
			mLastYaw = Yaw;

			ClientNative.sendcmd("euler --roll " + Roll + " --pitch " + Pitch + " --yaw " + Yaw + ";\r\n");
			Log.v("MainActivity", "Roll:" + Roll + ", Pitch:" + Pitch + ", Yaw:" + Yaw);

		} else if (event.sensor.getType() == Sensor.TYPE_ORIENTATION) {
			int y = (int) event.values[1];
			int z = (int) event.values[2];

			mOrientationView.update(y, z);
		}
	}

	@Override
	public void onAccuracyChanged(Sensor sensor, int accuracy) {
		
	}

	@Override
	public void onProgressChanged(SeekBar seekBar, int progress, boolean fromUser) {
		if (!mConnected) {
			return;
		}

		if (seekBar == mThrottleView) {
			ClientNative.sendcmd("throttle " + progress + ";\r\n");
			Log.v("MainActivity", "Throttle:" + progress);
		} else if (seekBar == mAltitudeView) {
			ClientNative.sendcmd("altitude " + progress + ";\r\n");
			Log.v("MainActivity", "Altitude:" + progress);
		}

	}

	@Override
	public void onStartTrackingTouch(SeekBar seekBar) {

	}

	@Override
	public void onStopTrackingTouch(SeekBar seekBar) {

		if (seekBar == mThrottleView) {
			Toast.makeText(getApplicationContext(), "Throttle " + seekBar.getProgress(), Toast.LENGTH_SHORT).show();
		} else if (seekBar == mAltitudeView) {
			Toast.makeText(getApplicationContext(), "Altitude " + seekBar.getProgress(), Toast.LENGTH_SHORT).show();
		}
	}
}
