package com.gfs.planecontroller;

import android.annotation.SuppressLint;
import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.util.AttributeSet;
import android.view.View;

public class OrientationView extends View {
	public int mX, mY;
	
	public OrientationView(Context context, AttributeSet attrs) {
		super(context, attrs);
		
	}

	@SuppressLint("DrawAllocation")
	@Override
	protected void onDraw(Canvas canvas) {
		super.onDraw(canvas);
		
		// 创建画笔
		Paint p = new Paint();
		p.setColor(Color.RED);// 设置红色
		
		drawCoordinate(canvas, p);
	}
	
	private void drawCoordinate(Canvas canvas, Paint paint) {
		int width = getWidth();
		int height = getHeight();
		
		canvas.drawLine(width/2, 0, width/2, height, paint);
		canvas.drawLine(0, height/2, width, height/2, paint);
		
		paint.setAntiAlias(true);
		paint.setStyle(Paint.Style.STROKE);
		canvas.drawCircle(width/2, height/2, width/2, paint);
		
		drawPlane(canvas, paint);
	}

	private void drawPlane(Canvas canvas, Paint paint) {
		int[] position = getPlanePosition();
		
		paint.setColor(Color.BLUE);
		paint.setStyle(Paint.Style.FILL);
		canvas.drawCircle(position[0], position[1], 50, paint);
	}
	
	private int[] getPlanePosition() {
		int[] pos = new int[2];
		int radias = getWidth()/2;
		int offsetX = (int)(Math.abs(mX)*radias/45);
		int offsetY = (int)(Math.abs(mY)*radias/45);
		
		pos[0] = getWidth()/2 - ((mY>0)?offsetY:-offsetY);
		pos[1] = getHeight()/2 - ((mX>0)?offsetX:-offsetX);
		
		return pos;
	}
	
	public void update(int x, int y) {
		mX = x;
		mY = y;
		invalidate();
	}
}
