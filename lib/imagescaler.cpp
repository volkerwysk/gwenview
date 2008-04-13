/*
Gwenview: an image viewer
Copyright 2007 Aurélien Gâteau <aurelien.gateau@free.fr>

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/
#include "imagescaler.moc"

#include <math.h>

// Qt
#include <QImage>
#include <QRegion>
#include <QTimer>

// KDE
#include <kdebug.h>

// Local
#include <lib/document/document.h>

#undef ENABLE_LOG
#undef LOG
//#define ENABLE_LOG
#ifdef ENABLE_LOG
#define LOG(x) kDebug() << x
#else
#define LOG(x) ;
#endif

namespace Gwenview {

// Amount of pixels to keep so that smooth scale is correct
static const int SMOOTH_MARGIN = 3;

struct ImageScalerPrivate {
	Qt::TransformationMode mTransformationMode;
	Document::Ptr mDocument;
	qreal mZoom;
	QRegion mRegion;
};

ImageScaler::ImageScaler(QObject* parent)
: QObject(parent)
, d(new ImageScalerPrivate) {
	d->mTransformationMode = Qt::FastTransformation;
}

ImageScaler::~ImageScaler() {
	delete d;
}

void ImageScaler::setDocument(Document::Ptr document) {
	d->mDocument = document;
}

void ImageScaler::setZoom(qreal zoom) {
	d->mZoom = zoom;
}

void ImageScaler::setTransformationMode(Qt::TransformationMode mode) {
	d->mTransformationMode = mode;
}

void ImageScaler::setDestinationRegion(const QRegion& region) {
	LOG(region);
	d->mRegion = region;
	if (d->mRegion.isEmpty()) {
		return;
	}

	if (d->mDocument) {
		doScale();
	}
}


QRect ImageScaler::containingRect(const QRectF& rectF) {
	return QRect(
		QPoint(
			qRound(floor(rectF.left())),
			qRound(floor(rectF.top()))
			),
		QPoint(
			qRound(ceil(rectF.right() - 1.)),
			qRound(ceil(rectF.bottom() - 1.))
			)
		);
	// Note: QRect::right = left + width - 1, while QRectF::right = left + width
}


void ImageScaler::doScale() {
	LOG("Starting");
	Q_FOREACH(const QRect& rect, d->mRegion.rects()) {
		LOG(rect);
		scaleRect(rect);
	}
	LOG("Done");
}


void ImageScaler::scaleRect(const QRect& rect) {
	const qreal REAL_DELTA = 0.001;
	if (qAbs(d->mZoom - 1.0) < REAL_DELTA) {
		QImage tmp = d->mDocument->image().copy(rect);
		tmp.convertToFormat(QImage::Format_ARGB32_Premultiplied);
		scaledRect(rect.left(), rect.top(), tmp);
		return;
	}

	QImage image;
	qreal zoom;
	if (d->mZoom < 1) {
		image = d->mDocument->downSampledImage(d->mZoom);
		qreal zoom1 = qreal(image.width()) / d->mDocument->width();
		zoom = d->mZoom / zoom1;
	} else {
		image = d->mDocument->image();
		zoom = d->mZoom;
	}
	// If rect contains "half" pixels, make sure sourceRect includes them
	QRectF sourceRectF(
		rect.left() / zoom,
		rect.top() / zoom,
		rect.width() / zoom,
		rect.height() / zoom);

	sourceRectF = sourceRectF.intersected(image.rect());
	QRect sourceRect = containingRect(sourceRectF);
	if (sourceRect.isEmpty()) {
		return;
	}

	// Compute smooth margin
	bool needsSmoothMargins = d->mTransformationMode == Qt::SmoothTransformation;

	int sourceLeftMargin, sourceRightMargin, sourceTopMargin, sourceBottomMargin;
	int destLeftMargin, destRightMargin, destTopMargin, destBottomMargin;
	if (needsSmoothMargins) {
		sourceLeftMargin = qMin(sourceRect.left(), SMOOTH_MARGIN);
		sourceTopMargin = qMin(sourceRect.top(), SMOOTH_MARGIN);
		sourceRightMargin = qMin(image.rect().right() - sourceRect.right(), SMOOTH_MARGIN);
		sourceBottomMargin = qMin(image.rect().bottom() - sourceRect.bottom(), SMOOTH_MARGIN);
		sourceRect.adjust(
			-sourceLeftMargin,
			-sourceTopMargin,
			sourceRightMargin,
			sourceBottomMargin);
		destLeftMargin = int(sourceLeftMargin * zoom);
		destTopMargin = int(sourceTopMargin * zoom);
		destRightMargin = int(sourceRightMargin * zoom);
		destBottomMargin = int(sourceBottomMargin * zoom);
	} else {
		sourceLeftMargin = sourceRightMargin = sourceTopMargin = sourceBottomMargin = 0;
		destLeftMargin = destRightMargin = destTopMargin = destBottomMargin = 0;
	}

	// destRect is almost like rect, but it contains only "full" pixels
	QRectF destRectF = QRectF(
		sourceRect.left() * zoom,
		sourceRect.top() * zoom,
		sourceRect.width() * zoom,
		sourceRect.height() * zoom
		);
	QRect destRect = containingRect(destRectF);

	QImage tmp;
	tmp = image.copy(sourceRect);
	tmp.convertToFormat(QImage::Format_ARGB32_Premultiplied);
	tmp = tmp.scaled(
		destRect.width(),
		destRect.height(),
		Qt::IgnoreAspectRatio, // Do not use KeepAspectRatio, it can lead to skipped rows or columns
		d->mTransformationMode);

	if (needsSmoothMargins) {
		tmp = tmp.copy(
			destLeftMargin, destTopMargin,
			destRect.width() - (destLeftMargin + destRightMargin),
			destRect.height() - (destTopMargin + destBottomMargin)
			);
	}

	scaledRect(destRect.left() + destLeftMargin, destRect.top() + destTopMargin, tmp);
}

} // namespace
