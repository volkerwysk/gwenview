// vim: set tabstop=4 shiftwidth=4 noexpandtab:
/*
Gwenview: an image viewer
Copyright 2009 Aurélien Gâteau <agateau@kde.org>

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
Foundation, Inc., 51 Franklin Street, Fifth Floor, Cambridge, MA 02110-1301, USA.

*/
// Self
#include "renamer.h"

// Qt
#include <QFileInfo>

// KDE
#include <kdatetime.h>
#include <kurl.h>

// Local

namespace Gwenview {

typedef QHash<QString, QString> Dict;

struct RenamerPrivate {
	QString mFormat;
};


Renamer::Renamer(const QString& format)
: d(new RenamerPrivate) {
	d->mFormat = format;
}


Renamer::~Renamer() {
	delete d;
}


QString Renamer::rename(const KUrl& url, const KDateTime& dateTime) {
	QFileInfo info(url.fileName());

	Dict dict;
	dict["date"]       = dateTime.toString("%Y-%m-%d");
	dict["time"]       = dateTime.toString("%H-%M-%S");
	dict["ext"]        = info.completeSuffix();
	dict["ext:lower"]  = info.completeSuffix().toLower();
	dict["name"]       = info.baseName();
	dict["name:lower"] = info.baseName().toLower();

	QString name;
	int length = d->mFormat.length();
	for (int pos=0; pos < length; ++pos) {
		QChar ch = d->mFormat[pos];
		if (ch == '{') {
			if (pos == length - 1) {
				// We are at the end, ignore this
				break;
			}
			if (d->mFormat[pos + 1] == '{') {
				// This is an escaped '{', skip one
				name += '{';
				++pos;
				continue;
			}
			int end = d->mFormat.indexOf('}', pos + 1);
			if (end == -1) {
				// No '}' found!
				continue;
			}
			QString keyword = d->mFormat.mid(pos + 1, end - pos - 1);
			Dict::ConstIterator it = dict.find(keyword);
			if (it == dict.constEnd()) {
				// No matching keyword, echo as is
				name += '{';
			} else {
				name += it.value();
				pos = end;
			}
		} else {
			name += ch;
		}
	}
	return name;
}


} // namespace
