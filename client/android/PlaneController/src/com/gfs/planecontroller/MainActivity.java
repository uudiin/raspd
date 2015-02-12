package com.gfs.planecontroller;

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
import android.widget.SeekBar;
import android.widget.SeekBar.OnSeekBarChangeListener;
import android.widget.TextView;

public class MainActivity extends Activity implements SensorEventListener, OnSeekBarChangeListener {
	private SensorManager mSensorMgr;
	private Sensor mSensor, mOrientationSensor;

	private TextView mXText, mYText, mZText;
	private OrientationView mOrientationView;
	private SeekBar mHeightView;
	
	@Override
	protected void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		setContentView(R.layout.activity_main);
		
		mOrientationView = (OrientationView)findViewById(R.id.planeview);
		mHeightView = (SeekBar)findViewById(R.id.heightview);
		mHeightView.setOnSeekBarChangeListener(this);
		
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
//			int x = (int) event.values[0];
//			int y = (int) event.values[1];
//			int z = (int) event.values[2];
//
//			mXText.setText(String.valueOf(x));
//			mYText.setText(String.valueOf(y));
//			mZText.setText(String.valueOf(z));
		}
		else if (event.sensor.getType() == Sensor.TYPE_ORIENTATION) {
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
