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
	private EditText mHostEditor;

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
		mHostEditor = (EditText)findViewById(R.id.addrview);
	    mHostEditor.setText("172.16.20.1");

		mConnectBtn.setOnClickListener(new View.OnClickListener() {

				@Override
				public void onClick(View arg0) {
					String host = mHostEditor.getEditableText().toString();
					int err = ClientNative.connect(host, 8888);
					if(0 == err) {
						Toast.makeText(getApplicationContext(), "Connect success", Toast.LENGTH_SHORT).show();
					}
					else {
						Toast.makeText(getApplicationContext(), "Error " + err, Toast.LENGTH_SHORT).show();
					}
				}
			});

		initSensor();
	}

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

	@Override
	public void onSensorChanged(SensorEvent event) {
		if (event.sensor == null) {
			return;
		}

		if (event.sensor.getType() == Sensor.TYPE_ACCELEROMETER) {
			int x = (int) event.values[0];
			int y = (int) event.values[1];
			int z = (int) event.values[2];
			
			Log.v("MainActivity", "(" + x + ", " + y + ", " + z + ")");

//
//			mXText.setText(String.valueOf(x));
//			mYText.setText(String.valueOf(y));
//			mZText.setText(String.valueOf(z));
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
		
	}

	@Override
	public void onStartTrackingTouch(SeekBar seekBar) {
		// TODO Auto-generated method stub
		
	}

	@Override
	public void onStopTrackingTouch(SeekBar seekBar) {
		// TODO Auto-generated method stub
		
	}
}
