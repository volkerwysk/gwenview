// vim: set tabstop=4 shiftwidth=4 noexpandtab
/*
Gwenview - A simple image viewer for KDE
Copyright 2000-2003 Aur�lien G�teau

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
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#ifndef GVSLIDESHOWDIALOG_H
#define GVSLIDESHOWDIALOG_H

// KDE includes
#include <kdialogbase.h>

class GVSlideShow;
class GVSlideShowDialogBase;

class GVSlideShowDialog: public KDialogBase {
Q_OBJECT
public:
	GVSlideShowDialog(QWidget* parent,GVSlideShow*);

protected slots:
	void slotOk();
	
private:
	GVSlideShowDialogBase* mContent;
	GVSlideShow* mSlideShow;
};


#endif
