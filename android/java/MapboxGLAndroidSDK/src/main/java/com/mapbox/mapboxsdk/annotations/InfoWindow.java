package com.mapbox.mapboxsdk.annotations;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.PointF;
import android.view.LayoutInflater;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import com.mapbox.mapboxsdk.R;
import com.mapbox.mapboxsdk.geometry.LatLng;
import com.mapbox.mapboxsdk.views.MapView;

import java.lang.ref.WeakReference;

/**
 * A tooltip view
 */
final class InfoWindow {

	static int mTitleId          = 0;
	static int mDescriptionId    = 0;
	static int mSubDescriptionId = 0;
	static int mImageId          = 0;
	protected View                   mView;
	private   WeakReference<Marker>  mBoundMarker;
	private   WeakReference<MapView> mMapView;
	private   boolean                mIsVisible;

	public InfoWindow(int layoutResId, MapView mapView) {
		mMapView = new WeakReference<MapView>(mapView);
		mIsVisible = false;
		mView = LayoutInflater.from(mapView.getContext()).inflate(layoutResId, mapView, false);

		if (mTitleId == 0) {
			setResIds(mapView.getContext());
		}

		// default behavior: close it when clicking on the tooltip:
		setOnTouchListener(new View.OnTouchListener() {
			@Override
			public boolean onTouch(View v, MotionEvent e) {
				if (e.getAction() == MotionEvent.ACTION_UP) {
					close();
				}
				return true;
			}
		});
	}

	public InfoWindow(View view, MapView mapView) {
		mMapView = new WeakReference<MapView>(mapView);
		mIsVisible = false;
		mView = view;

		// default behavior: close it when clicking on the tooltip:
		setOnTouchListener(new View.OnTouchListener() {
			@Override
			public boolean onTouch(View v, MotionEvent e) {
				if (e.getAction() == MotionEvent.ACTION_UP) {
					close();
				}
				return true;
			}
		});
	}

	/**
	 * Given a context, set the resource ids for the layout
	 * of the InfoWindow.
	 *
	 * @param context
	 */
	private static void setResIds(Context context) {
		String packageName = context.getPackageName(); //get application package name
		mTitleId = context.getResources().getIdentifier("id/infowindow_title", null, packageName);
		mDescriptionId =
				context.getResources().getIdentifier("id/infowindow_description", null, packageName);
		mSubDescriptionId = context.getResources()
				.getIdentifier("id/infowindow_subdescription", null, packageName);
		mImageId = context.getResources().getIdentifier("id/infowindow_image", null, packageName);
	}

	/**
	 * open the window at the specified position.
	 *
	 * @param object   the graphical object on which is hooked the view
	 * @param position to place the window on the map
	 * @param offsetX  (&offsetY) the offset of the view to the position, in pixels.
	 *                 This allows to offset the view from the object position.
	 * @return this infowindow
	 */
	public InfoWindow open(Marker object, LatLng position, int offsetX, int offsetY) {
		MapView.LayoutParams lp = new MapView.LayoutParams(MapView.LayoutParams.WRAP_CONTENT, MapView.LayoutParams.WRAP_CONTENT);
		mView.measure(View.MeasureSpec.UNSPECIFIED, View.MeasureSpec.UNSPECIFIED);

		// Calculate default Android x,y coordinate
		PointF coords = mMapView.get().toScreenLocation(position);
		float x = coords.x - (mView.getMeasuredWidth() / 2) + offsetX;
		float y = coords.y - mView.getMeasuredHeight() + offsetY;

		// get right/left popup window
		float right = x + mView.getMeasuredWidth();
		float left = x;

		// get right/left map view
		float mapRight = mMapView.get().getRight();
		float mapLeft = mMapView.get().getLeft();

		if (mView instanceof InfoWindowView) {
			// only apply repositioning/margin for InfoWindowView
			Resources resources = mMapView.get().getContext().getResources();
			float margin = resources.getDimension(R.dimen.infowindow_margin);
			float tipViewOffset = resources.getDimension(R.dimen.infowindow_tipview_width) / 2;
			float tipViewMarginLeft = mView.getMeasuredWidth() / 2 - tipViewOffset;

			// fit screen on right
			if (right > mMapView.get().getRight()) {
				x -= right - mapRight;
				tipViewMarginLeft += right - mapRight + tipViewOffset;
				right = x + mView.getMeasuredWidth();
			}

			// fit screen left
			if (left < mMapView.get().getLeft()) {
				x += mapLeft - left;
				tipViewMarginLeft -= mapLeft - left + tipViewOffset;
				left = x;
			}

			// Add margin right
			if (mapRight - right < margin) {
				x -= margin - (mapRight - right);
				tipViewMarginLeft += margin - (mapRight - right) - tipViewOffset;
				left = x;
			}

			// Add margin left
			if (left - mapLeft < margin) {
				x += margin - (left - mapLeft);
				tipViewMarginLeft -= (margin - (left - mapLeft)) - tipViewOffset;
			}

			// Adjust tipView
			InfoWindowView infoWindowView = (InfoWindowView) mView;
			infoWindowView.setTipViewMarginLeft((int) tipViewMarginLeft);
		}

		// set anchor popupwindowview
		mView.setX(x);
		mView.setY(y);

		close(); //if it was already opened
		mMapView.get().addView(mView, lp);
		mIsVisible = true;
		return this;
	}

	/**
	 * Close this InfoWindow if it is visible, otherwise don't do anything.
	 *
	 * @return this info window
	 */
	public InfoWindow close() {
		if (mIsVisible) {
			mIsVisible = false;
			((ViewGroup) mView.getParent()).removeView(mView);
//            this.boundMarker.blur();
			setBoundMarker(null);
			onClose();
		}
		return this;
	}

	/**
	 * Returns the Android view. This allows to set its content.
	 *
	 * @return the Android view
	 */
	public View getView() {
		return mView;
	}

	/**
	 * Returns the mapView this InfoWindow is bound to
	 *
	 * @return the mapView
	 */
	public MapView getMapView() {
		return mMapView.get();
	}

	/**
	 * Constructs the view that is displayed when the InfoWindow opens.
	 * This retrieves data from overlayItem and shows it in the tooltip.
	 *
	 * @param overlayItem the tapped overlay item
	 */
	public void adaptDefaultMarker(Marker overlayItem) {
		String title = overlayItem.getTitle();
		((TextView) mView.findViewById(mTitleId /*R.id.title*/)).setText(title);
		String snippet = overlayItem.getSnippet();
		((TextView) mView.findViewById(mDescriptionId /*R.id.description*/)).setText(snippet);

/*
        //handle sub-description, hiding or showing the text view:
        TextView subDescText = (TextView) mView.findViewById(mSubDescriptionId);
        String subDesc = overlayItem.getSubDescription();
        if ("".equals(subDesc)) {
            subDescText.setVisibility(View.GONE);
        } else {
            subDescText.setText(subDesc);
            subDescText.setVisibility(View.VISIBLE);
        }
*/
	}

	public void onClose() {
		//by default, do nothing
	}

	public Marker getBoundMarker() {
		if (mBoundMarker == null) {
			return null;
		}
		return mBoundMarker.get();
	}

	public InfoWindow setBoundMarker(Marker boundMarker) {
		mBoundMarker = new WeakReference<Marker>(boundMarker);
		return this;
	}

	/**
	 * Use to override default touch events handling on InfoWindow (ie, close automatically)
	 *
	 * @param listener New View.OnTouchListener to use
	 */
	public void setOnTouchListener(View.OnTouchListener listener) {
		mView.setOnTouchListener(listener);
	}
}
