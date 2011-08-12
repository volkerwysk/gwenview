/*
Gwenview: an image viewer
Copyright 2007 Aurélien Gâteau <agateau@kde.org>

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
#include "mainwindow.moc"
#include <config-gwenview.h>

// Qt
#include <QApplication>
#include <QDateTime>
#include <QDesktopWidget>
#include <QLabel>
#include <QTimer>
#include <QShortcut>
#include <QSplitter>
#include <QSlider>
#include <QStackedWidget>
#include <QUndoGroup>
#include <QVBoxLayout>

// KDE
#include <kio/netaccess.h>
#include <kactioncollection.h>
#include <kaction.h>
#include <kapplication.h>
#include <kdirlister.h>
#include <kedittoolbar.h>
#include <kfiledialog.h>
#include <kfileitem.h>
#include <kglobalsettings.h>
#include <kmenubar.h>
#include <klocale.h>
#include <kmessagebox.h>
#include <knotificationrestrictions.h>
#include <kprotocolmanager.h>
#include <kstatusbar.h>
#include <kstandardguiitem.h>
#include <kstandardshortcut.h>
#include <kactioncategory.h>
#include <kprotocolmanager.h>
#include <ktogglefullscreenaction.h>
#include <ktoolbar.h>
#include <kurl.h>
#include <kurlnavigator.h>
#include <kxmlguifactory.h>
#include <kwindowsystem.h>

// Local
#include "configdialog.h"
#include "contextmanager.h"
#include "documentinfoprovider.h"
#include "documentpanel.h"
#include "fileopscontextmanageritem.h"
#include "folderviewcontextmanageritem.h"
#include "fullscreencontent.h"
#include "gvcore.h"
#include "imageopscontextmanageritem.h"
#include "infocontextmanageritem.h"
#ifdef KIPI_FOUND
#include "kipiexportaction.h"
#include "kipiinterface.h"
#endif
#ifndef GWENVIEW_SEMANTICINFO_BACKEND_NONE
#ifdef GWENVIEW_SEMANTICINFO_BACKEND_NEPOMUK
#include <nepomuk/resourcemanager.h>
#endif
#include "semanticinfocontextmanageritem.h"
#endif
#include "preloader.h"
#include "savebar.h"
#include "sidebar.h"
#include "splitter.h"
#include "startpage.h"
#include "thumbnailviewhelper.h"
#include "thumbnailviewpanel.h"
#include <lib/archiveutils.h>
#include <lib/document/documentfactory.h>
#include <lib/eventwatcher.h>
#include <lib/fullscreenbar.h>
#include <lib/gwenviewconfig.h>
#include <lib/messagebubble.h>
#include <lib/mimetypeutils.h>
#include <lib/print/printhelper.h>
#include <lib/slideshow.h>
#include <lib/signalblocker.h>
#include <lib/semanticinfo/sorteddirmodel.h>
#include <lib/splittercollapser.h>
#include <lib/thumbnailview/thumbnailbarview.h>
#include <lib/thumbnailview/thumbnailview.h>
#include <lib/thumbnailloadjob.h>
#include <lib/urlutils.h>
#include <lib/widgetfloater.h>

namespace Gwenview {

#undef ENABLE_LOG
#undef LOG
//#define ENABLE_LOG
#ifdef ENABLE_LOG
#define LOG(x) kDebug() << x
#else
#define LOG(x) ;
#endif

static const int PRELOAD_DELAY = 1000;

static const char* BROWSE_MODE_SIDE_BAR_GROUP = "SideBar-BrowseMode";
static const char* VIEW_MODE_SIDE_BAR_GROUP = "SideBar-ViewMode";
static const char* FULLSCREEN_MODE_SIDE_BAR_GROUP = "SideBar-FullScreenMode";
static const char* SIDE_BAR_IS_VISIBLE_KEY = "IsVisible";

static const char* SESSION_CURRENT_PAGE_KEY = "Page";
static const char* SESSION_URL_KEY = "Url";

enum PageId {
	StartPageId,
	BrowsePageId,
	ViewPageId
};

struct MainWindowState {
	bool mToolBarVisible;
	Qt::WindowStates mWindowState;
};


struct MainWindow::Private {
	GvCore* mGvCore;
	MainWindow* mWindow;
	QSplitter* mCentralSplitter;
	QWidget* mContentWidget;
	DocumentPanel* mDocumentPanel;
	KUrlNavigator* mUrlNavigator;
	ThumbnailView* mThumbnailView;
	DocumentInfoProvider* mDocumentInfoProvider;
	ThumbnailViewHelper* mThumbnailViewHelper;
	ThumbnailViewPanel* mThumbnailViewPanel;
	StartPage* mStartPage;
	SideBar* mSideBar;
	SplitterCollapser* mSideBarCollapser;
	QStackedWidget* mViewStackedWidget;
	FullScreenBar* mFullScreenBar;
	FullScreenContent* mFullScreenContent;
	QPalette mFullScreenPalette;
	SaveBar* mSaveBar;
	bool mStartSlideShowWhenDirListerCompleted;
	SlideShow* mSlideShow;
	Preloader* mPreloader;
#ifdef KIPI_FOUND
	KIPIInterface* mKIPIInterface;
#endif

	QActionGroup* mViewModeActionGroup;
	KAction* mBrowseAction;
	KAction* mViewAction;
	KAction* mGoUpAction;
	KAction* mGoToPreviousAction;
	KAction* mGoToNextAction;
	KAction* mGoToFirstAction;
	KAction* mGoToLastAction;
	KToggleAction* mToggleSideBarAction;
	KToggleFullScreenAction* mFullScreenAction;
	KAction* mToggleSlideShowAction;
	KToggleAction* mShowMenuBarAction;
#ifdef KIPI_FOUND
	KIPIExportAction* mKIPIExportAction;
#endif

	SortedDirModel* mDirModel;
	ContextManager* mContextManager;

	bool mDistractionFreeMode;
	MainWindowState mStateBeforeFullScreen;

	KUrl mUrlToSelect;

	QString mCaption;

	PageId mCurrentPageId;

	QDateTime mFullScreenLeftAt;
	KNotificationRestrictions* mNotificationRestrictions;

	void setupWidgets() {
		KSharedConfigPtr config = KSharedConfig::openConfig("color-schemes/ObsidianCoast.colors", KConfig::FullConfig, "data");
		mFullScreenPalette = KGlobalSettings::createApplicationPalette(config);

		mCentralSplitter = new Splitter(Qt::Horizontal, mWindow);
		mWindow->setCentralWidget(mCentralSplitter);

		// Left side of splitter
		mSideBar = new SideBar(mCentralSplitter);
		EventWatcher::install(mSideBar, QList<QEvent::Type>() << QEvent::Show << QEvent::Hide,
			mWindow, SLOT(updateToggleSideBarAction()));

		// Right side of splitter
		mContentWidget = new QWidget(mCentralSplitter);

		mSaveBar = new SaveBar(mContentWidget, mWindow->actionCollection());
		mViewStackedWidget = new QStackedWidget(mContentWidget);
		QVBoxLayout* layout = new QVBoxLayout(mContentWidget);
		layout->addWidget(mSaveBar);
		layout->addWidget(mViewStackedWidget);
		layout->setMargin(0);
		layout->setSpacing(0);
		////

		mSideBarCollapser = new SplitterCollapser(mCentralSplitter, mSideBar);

		mStartSlideShowWhenDirListerCompleted = false;
		mSlideShow = new SlideShow(mWindow);

		setupThumbnailView(mViewStackedWidget);
		setupDocumentPanel(mViewStackedWidget);
		setupStartPage(mViewStackedWidget);
		mViewStackedWidget->addWidget(mThumbnailViewPanel);
		mViewStackedWidget->addWidget(mDocumentPanel);
		mViewStackedWidget->addWidget(mStartPage);
		mViewStackedWidget->setCurrentWidget(mThumbnailViewPanel);

		mCentralSplitter->setStretchFactor(0, 0);
		mCentralSplitter->setStretchFactor(1, 1);

		connect(mSaveBar, SIGNAL(requestSaveAll()),
			mGvCore, SLOT(saveAll()) );
		connect(mSaveBar, SIGNAL(goToUrl(KUrl)),
			mWindow, SLOT(goToUrl(KUrl)) );

		connect(mSlideShow, SIGNAL(goToUrl(KUrl)),
			mWindow, SLOT(goToUrl(KUrl)) );
	}

	void setupThumbnailView(QWidget* parent) {
		mThumbnailViewPanel = new ThumbnailViewPanel(parent, mDirModel, mWindow->actionCollection());

		mThumbnailView = mThumbnailViewPanel->thumbnailView();
		mUrlNavigator = mThumbnailViewPanel->urlNavigator();

		mDocumentInfoProvider = new DocumentInfoProvider(mDirModel);
		mThumbnailView->setDocumentInfoProvider(mDocumentInfoProvider);

		mThumbnailViewHelper = new ThumbnailViewHelper(mDirModel, mWindow->actionCollection());
		mThumbnailView->setThumbnailViewHelper(mThumbnailViewHelper);

		// Connect thumbnail view
		connect(mThumbnailView, SIGNAL(indexActivated(QModelIndex)),
			mWindow, SLOT(slotThumbnailViewIndexActivated(QModelIndex)) );
		connect(mThumbnailView->selectionModel(), SIGNAL(selectionChanged(QItemSelection,QItemSelection)),
			mWindow, SLOT(slotSelectionChanged()) );

		// Connect delegate
		QAbstractItemDelegate* delegate = mThumbnailView->itemDelegate();
		connect(delegate, SIGNAL(saveDocumentRequested(KUrl)),
			mGvCore, SLOT(save(KUrl)) );
		connect(delegate, SIGNAL(rotateDocumentLeftRequested(KUrl)),
			mGvCore, SLOT(rotateLeft(KUrl)) );
		connect(delegate, SIGNAL(rotateDocumentRightRequested(KUrl)),
			mGvCore, SLOT(rotateRight(KUrl)) );
		connect(delegate, SIGNAL(showDocumentInFullScreenRequested(KUrl)),
			mWindow, SLOT(showDocumentInFullScreen(KUrl)) );
		connect(delegate, SIGNAL(setDocumentRatingRequested(KUrl,int)),
			mGvCore, SLOT(setRating(KUrl,int)) );

		// Connect url navigator
		connect(mUrlNavigator, SIGNAL(urlChanged(KUrl)),
			mWindow, SLOT(openDirUrl(KUrl)) );
	}

	void setupDocumentPanel(QWidget* parent) {
		mDocumentPanel = new DocumentPanel(parent, mSlideShow, mWindow->actionCollection());
		connect(mDocumentPanel, SIGNAL(captionUpdateRequested(QString)),
			mWindow, SLOT(setCaption(QString)) );
		connect(mDocumentPanel, SIGNAL(completed()),
			mWindow, SLOT(slotPartCompleted()) );
		connect(mDocumentPanel, SIGNAL(previousImageRequested()),
			mWindow, SLOT(goToPrevious()) );
		connect(mDocumentPanel, SIGNAL(nextImageRequested()),
			mWindow, SLOT(goToNext()) );

		ThumbnailView* bar = mDocumentPanel->thumbnailBar();
		bar->setModel(mDirModel);
		bar->setDocumentInfoProvider(mDocumentInfoProvider);
		bar->setThumbnailViewHelper(mThumbnailViewHelper);
		bar->setSelectionModel(mThumbnailView->selectionModel());
	}

	void setupStartPage(QWidget* parent) {
		mStartPage = new StartPage(parent, mGvCore);
		connect(mStartPage, SIGNAL(urlSelected(KUrl)),
			mWindow, SLOT(slotStartPageUrlSelected(KUrl)) );
	}


	void setupActions() {
		KActionCollection* actionCollection = mWindow->actionCollection();
		KActionCategory* file = new KActionCategory(i18nc("@title actions category","File"), actionCollection);
		KActionCategory* view = new KActionCategory(i18nc("@title actions category - means actions changing smth in interface","View"), actionCollection);

		file->addAction(KStandardAction::Save,mWindow, SLOT(saveCurrent()));
		file->addAction(KStandardAction::SaveAs,mWindow, SLOT(saveCurrentAs()));
		file->addAction(KStandardAction::Open,mWindow, SLOT(openFile()));
		file->addAction(KStandardAction::Print,mWindow, SLOT(print()));
		file->addAction(KStandardAction::Quit,KApplication::kApplication(), SLOT(closeAllWindows()));

		KAction* action = file->addAction("reload",mWindow,SLOT(reload()));
		action->setText(i18nc("@action reload the currently viewed image", "Reload"));
		action->setIcon(KIcon("view-refresh"));
		action->setShortcut(Qt::Key_F5);

		mBrowseAction = view->addAction("browse");
		mBrowseAction->setText(i18nc("@action Switch to file list", "Browse"));
		mBrowseAction->setCheckable(true);
		mBrowseAction->setIcon(KIcon("view-list-icons"));

		mViewAction = view->addAction("view");
		mViewAction->setText(i18nc("@action Switch to image view", "View"));
		mViewAction->setIcon(KIcon("view-preview"));
		mViewAction->setCheckable(true);

		mViewModeActionGroup = new QActionGroup(mWindow);
		mViewModeActionGroup->addAction(mBrowseAction);
		mViewModeActionGroup->addAction(mViewAction);

		connect(mViewModeActionGroup, SIGNAL(triggered(QAction*)),
			mWindow, SLOT(setActiveViewModeAction(QAction*)) );

		mFullScreenAction = static_cast<KToggleFullScreenAction*>(view->addAction(KStandardAction::FullScreen,mWindow, SLOT(toggleFullScreen(bool))));
		KShortcut shortcut = mFullScreenAction->shortcut();
		if (shortcut.alternate().isEmpty()) {
			shortcut.setAlternate(Qt::Key_F11);
		}
		mFullScreenAction->setShortcut(shortcut);

		KAction* leaveFullScreenAction = view->addAction("leave_fullscreen", mWindow, SLOT(leaveFullScreen()));
		leaveFullScreenAction->setText(i18nc("@action", "Leave Fullscreen Mode"));
		leaveFullScreenAction->setShortcut(Qt::Key_Escape);

		connect(mDocumentPanel, SIGNAL(goToBrowseModeRequested()),
			mBrowseAction, SLOT(trigger()) );

		mGoToPreviousAction = view->addAction("go_previous",mWindow, SLOT(goToPrevious()));
		mGoToPreviousAction->setIcon(KIcon("media-skip-backward"));
		mGoToPreviousAction->setText(i18nc("@action Go to previous image", "Previous"));
		mGoToPreviousAction->setToolTip(i18n("Go to Previous Image"));
		mGoToPreviousAction->setShortcut(Qt::Key_Backspace);

		mGoToNextAction = view->addAction("go_next",mWindow, SLOT(goToNext()));
		mGoToNextAction->setIcon(KIcon("media-skip-forward"));
		mGoToNextAction->setText(i18nc("@action Go to next image", "Next"));
		mGoToNextAction->setToolTip(i18n("Go to Next Image"));
		mGoToNextAction->setShortcut(Qt::Key_Space);

		mGoToFirstAction = view->addAction("go_first",mWindow, SLOT(goToFirst()));
		mGoToFirstAction->setText(i18nc("@action Go to first image", "First"));
		mGoToFirstAction->setToolTip(i18n("Go to First Image"));
		mGoToFirstAction->setShortcut(Qt::Key_Home);

		mGoToLastAction = view->addAction("go_last",mWindow, SLOT(goToLast()));
		mGoToLastAction->setText(i18nc("@action Go to last image", "Last"));
		mGoToLastAction->setToolTip(i18n("Go to Last Image"));
		mGoToLastAction->setShortcut(Qt::Key_End);

		mGoUpAction = view->addAction(KStandardAction::Up,mWindow, SLOT(goUp()));

		action = view->addAction("go_start_page",mWindow, SLOT(showStartPage()));
		action->setIcon(KIcon("go-home"));
		action->setText(i18nc("@action", "Start Page"));

		mToggleSideBarAction = view->add<KToggleAction>("toggle_sidebar");
		connect(mToggleSideBarAction, SIGNAL(toggled(bool)),
			mWindow, SLOT(toggleSideBar(bool)));
		mToggleSideBarAction->setIcon(KIcon("view-sidetree"));
		mToggleSideBarAction->setShortcut(Qt::Key_F4);
		mToggleSideBarAction->setText(i18nc("@action", "Sidebar"));

		mToggleSlideShowAction = view->addAction("toggle_slideshow",mWindow, SLOT(toggleSlideShow()));
		mWindow->updateSlideShowAction();
		connect(mSlideShow, SIGNAL(stateChanged(bool)),
			mWindow, SLOT(updateSlideShowAction()) );

		mShowMenuBarAction = static_cast<KToggleAction*>(view->addAction(KStandardAction::ShowMenubar,mWindow, SLOT(toggleMenuBar())));

		view->addAction(KStandardAction::KeyBindings,mWindow->guiFactory(),
			SLOT(configureShortcuts()));

		view->addAction(KStandardAction::Preferences,mWindow,
			SLOT(showConfigDialog()));

		view->addAction(KStandardAction::ConfigureToolbars,mWindow,
			SLOT(configureToolbars()));

#ifdef KIPI_FOUND
		mKIPIExportAction = new KIPIExportAction(mWindow);
		actionCollection->addAction("kipi_export", mKIPIExportAction);
#endif
	}

	void setupUndoActions() {
		// There is no KUndoGroup similar to KUndoStack. This code basically
		// does the same as KUndoStack, but for the KUndoGroup actions.
		QUndoGroup* undoGroup = DocumentFactory::instance()->undoGroup();
		QAction* action;
		KActionCollection* actionCollection =  mWindow->actionCollection();
		KActionCategory* edit = new KActionCategory(i18nc("@title actions category - means actions changing smth in interface","Edit"), actionCollection);

		action = undoGroup->createRedoAction(actionCollection);
		action->setObjectName(KStandardAction::name(KStandardAction::Redo));
		action->setIcon(KIcon("edit-redo"));
		action->setIconText(i18n("Redo"));
		action->setShortcuts(KStandardShortcut::redo());
		edit->addAction(action->objectName(), action);

		action = undoGroup->createUndoAction(actionCollection);
		action->setObjectName(KStandardAction::name(KStandardAction::Undo));
		action->setIcon(KIcon("edit-undo"));
		action->setIconText(i18n("Undo"));
		action->setShortcuts(KStandardShortcut::undo());
		edit->addAction(action->objectName(), action);
	}


	void setupContextManager() {
		KActionCollection* actionCollection = mWindow->actionCollection();

		mContextManager = new ContextManager(mDirModel, mThumbnailView->selectionModel(), mWindow);

		// Create context manager items
		FolderViewContextManagerItem* folderViewItem = new FolderViewContextManagerItem(mContextManager);
		mContextManager->addItem(folderViewItem);
		connect(folderViewItem, SIGNAL(urlChanged(KUrl)),
			mWindow, SLOT(openDirUrl(KUrl)));

		InfoContextManagerItem* infoItem = new InfoContextManagerItem(mContextManager);
		mContextManager->addItem(infoItem);

		#ifndef GWENVIEW_SEMANTICINFO_BACKEND_NONE
		SemanticInfoContextManagerItem* semanticInfoItem = 0;
		// When using Nepomuk, create a SemanticInfoContextManagerItem instance
		// only if Nepomuk is available
		// When using Fake backend always create it
		#ifdef GWENVIEW_SEMANTICINFO_BACKEND_NEPOMUK
		if (Nepomuk::ResourceManager::instance()->init() == 0) {
		#endif
			semanticInfoItem = new SemanticInfoContextManagerItem(mContextManager, actionCollection, mDocumentPanel);
		#ifdef GWENVIEW_SEMANTICINFO_BACKEND_NEPOMUK
		}
		#endif
		if (semanticInfoItem) {
			mContextManager->addItem(semanticInfoItem);
		}
		#endif

		ImageOpsContextManagerItem* imageOpsItem =
			new ImageOpsContextManagerItem(mContextManager, mWindow);
		mContextManager->addItem(imageOpsItem);

		FileOpsContextManagerItem* fileOpsItem = new FileOpsContextManagerItem(mContextManager, mThumbnailView, actionCollection, mWindow);
		mContextManager->addItem(fileOpsItem);

		// Fill sidebar
		SideBarPage* page;
		page = new SideBarPage(i18n("Folders"));
		page->setObjectName( QLatin1String("folders" ));
		page->addWidget(folderViewItem->widget());
		page->layout()->setMargin(0);
		mSideBar->addPage(page);

		page = new SideBarPage(i18n("Information"));
		page->setObjectName( QLatin1String("information" ));
		page->addWidget(infoItem->widget());
		#ifndef GWENVIEW_SEMANTICINFO_BACKEND_NONE
		if (semanticInfoItem) {
			page->addWidget(semanticInfoItem->widget());
		}
		#endif
		page->addStretch();
		mSideBar->addPage(page);

		page = new SideBarPage(i18n("Operations"));
		page->setObjectName( QLatin1String("operations" ));
		page->addWidget(imageOpsItem->widget());
		page->addWidget(fileOpsItem->widget());
		page->addStretch();
		mSideBar->addPage(page);
	}


	void initDirModel() {
		mDirModel->setKindFilter(
			MimeTypeUtils::KIND_RASTER_IMAGE
			| MimeTypeUtils::KIND_SVG_IMAGE
			| MimeTypeUtils::KIND_VIDEO);
		setDirModelShowDirs(true);

		connect(mDirModel, SIGNAL(rowsInserted(QModelIndex,int,int)),
			mWindow, SLOT(slotDirModelNewItems()) );

		connect(mDirModel, SIGNAL(rowsRemoved(QModelIndex,int,int)),
			mWindow, SLOT(updatePreviousNextActions()) );
		connect(mDirModel, SIGNAL(modelReset()),
			mWindow, SLOT(updatePreviousNextActions()) );

		connect(mDirModel->dirLister(), SIGNAL(completed()),
			mWindow, SLOT(slotDirListerCompleted()) );
	}

	void setDirModelShowDirs(bool showDirs) {
		mDirModel->adjustKindFilter(MimeTypeUtils::KIND_DIR | MimeTypeUtils::KIND_ARCHIVE, showDirs);
	}

	QModelIndex currentIndex() const {
		KUrl url = currentUrl();
		return url.isValid() ? mDirModel->indexForUrl(url) : QModelIndex();
	}

	bool indexIsDirOrArchive(const QModelIndex& index) const {
		Q_ASSERT(index.isValid());
		KFileItem item = mDirModel->itemForIndex(index);
		return ArchiveUtils::fileItemIsDirOrArchive(item);
	}

	void goTo(const QModelIndex& index) {
		if (!index.isValid()) {
			return;
		}
		mThumbnailView->setCurrentIndex(index);
		mThumbnailView->scrollTo(index);
	}


	void goTo(int offset) {
		QModelIndex index = currentIndex();
		index = mDirModel->index(index.row() + offset, 0);
		if (!index.isValid()) {
			return;
		}
		if (!indexIsDirOrArchive(index)) {
			goTo(index);
		}
	}

	void goToFirstDocument() {
		QModelIndex index;
		for(int row=0;; ++row) {
			index = mDirModel->index(row, 0);
			if (!index.isValid()) {
				return;
			}

			if (!indexIsDirOrArchive(index)) {
				break;
			}
		}
		goTo(index);
	}

	void goToLastDocument() {
		QModelIndex index = mDirModel->index(mDirModel->rowCount() - 1, 0);
		goTo(index);
	}

	void spreadCurrentDirUrl(const KUrl& url) {
		mContextManager->setCurrentDirUrl(url);
		mThumbnailViewHelper->setCurrentDirUrl(url);
		if (url.isValid()) {
			mUrlNavigator->setLocationUrl(url);
			mGoUpAction->setEnabled(url.path() != "/");
			mGvCore->addUrlToRecentFolders(url);
		} else {
			mGoUpAction->setEnabled(false);
		}
	}

	void setupFullScreenBar() {
		mFullScreenBar = new FullScreenBar(mDocumentPanel);
		mFullScreenContent = new FullScreenContent(
			mFullScreenBar, mWindow->actionCollection(), mSlideShow);

		ThumbnailBarView* view = mFullScreenContent->thumbnailBar();
		view->setModel(mDirModel);
		view->setDocumentInfoProvider(mDocumentInfoProvider);
		view->setThumbnailViewHelper(mThumbnailViewHelper);
		view->setSelectionModel(mThumbnailView->selectionModel());
	}


	inline void setActionEnabled(const char* name, bool enabled) {
		QAction* action = mWindow->actionCollection()->action(name);
		if (action) {
			action->setEnabled(enabled);
		} else {
			kWarning() << "Action" << name << "not found";
		}
	}


	void setActionsDisabledOnStartPageEnabled(bool enabled) {
		mBrowseAction->setEnabled(enabled);
		mViewAction->setEnabled(enabled);
		mToggleSideBarAction->setEnabled(enabled);
		mFullScreenAction->setEnabled(enabled);
		mToggleSlideShowAction->setEnabled(enabled);

		setActionEnabled("reload", enabled);
		setActionEnabled("go_start_page", enabled);
		setActionEnabled("add_folder_to_places", enabled);
	}

	void updateDistractionsState() {
		const bool isFullScreen = mFullScreenAction->isChecked();
		mSideBarCollapser->setVisible(!mDistractionFreeMode && !isFullScreen);
		mFullScreenBar->setActivated(!mDistractionFreeMode && isFullScreen);
	}

	void updateActions() {
		bool isRasterImage = false;
		bool canSave = false;
		bool isModified = false;
		const KUrl url = currentUrl();

		if (url.isValid()) {
			isRasterImage = mWindow->currentDocumentIsRasterImage();
			canSave = isRasterImage;
			isModified = DocumentFactory::instance()->load(url)->isModified();
			if (mCurrentPageId != ViewPageId) {
				// Saving only makes sense if exactly one image is selected
				QItemSelection selection = mThumbnailView->selectionModel()->selection();
				QModelIndexList indexList = selection.indexes();
				if (indexList.count() != 1) {
					canSave = false;
				}
			}
		}

		KActionCollection* actionCollection = mWindow->actionCollection();
		actionCollection->action("file_save")->setEnabled(canSave && isModified);
		actionCollection->action("file_save_as")->setEnabled(canSave);
		actionCollection->action("file_print")->setEnabled(isRasterImage);
	}

	KUrl currentUrl() const {
		if (mCurrentPageId == StartPageId) {
			return KUrl();
		}

		if (mUrlToSelect.isValid()) {
			// We are supposed to select this url whenever it appears in the
			// dir model. For everyone it is as if it was already the current one
			return mUrlToSelect;
		}

		// mThumbnailViewPanel and mDocumentPanel urls are almost always synced, but
		// mThumbnailViewPanel can be more up-to-date because mDocumentPanel
		// url is only updated when the DocumentView starts to load the
		// document.
		// This is why we only thrust mDocumentPanel url if it shows an url
		// which can't be listed: in this case mThumbnailViewPanel url is
		// empty.
		if (mCurrentPageId == ViewPageId && !mDocumentPanel->isEmpty()) {
			KUrl url = mDocumentPanel->url();
			if (!KProtocolManager::supportsListing(url)) {
				return url;
			}
		}

		QModelIndex index = mThumbnailView->currentIndex();
		if (!index.isValid()) {
			return KUrl();
		}
		// Ignore the current index if it's not part of the selection. This
		// situation can happen when you select an image, then click on the
		// background of the thumbnail view.
		if (mCurrentPageId == BrowsePageId && !mThumbnailView->selectionModel()->isSelected(index)) {
			return KUrl();
		}
		KFileItem item = mDirModel->itemForIndex(index);
		Q_ASSERT(!item.isNull());
		return item.url();
	}

	void setUrlToSelect(const KUrl& url) {
		if (!url.isValid()) {
			kWarning() << "called with an invalid url, this should not happen!";
			return;
		}
		mUrlToSelect = url;
		updateContextDependentComponents();
		selectUrlToSelect();
	}

	void selectUrlToSelect() {
		QModelIndex index = mDirModel->indexForUrl(mUrlToSelect);
		if (index.isValid()) {
			if (index != mThumbnailView->currentIndex()) { // Avoid infinite recursion. Bug #243091
				mThumbnailView->setCurrentIndex(index);
			}
			mThumbnailView->scrollTo(index, QAbstractItemView::PositionAtCenter);
			mUrlToSelect = KUrl();
		}
	}

	void updateContextDependentComponents() {
		KUrl url = currentUrl();
		mContextManager->setCurrentUrl(url);
		mSaveBar->setCurrentUrl(url);
		mSlideShow->setCurrentUrl(url);
		mFullScreenContent->setCurrentUrl(url);
	}

	const char* sideBarConfigGroupName() const {
		const char* name = 0;
		switch (mCurrentPageId) {
		case StartPageId:
			kWarning() << "Should not happen!";
			// Fall through
		case BrowsePageId:
			name = BROWSE_MODE_SIDE_BAR_GROUP;
			break;
		case ViewPageId:
			name = mDocumentPanel->isFullScreenMode()
				? FULLSCREEN_MODE_SIDE_BAR_GROUP
				: VIEW_MODE_SIDE_BAR_GROUP;
			break;
		}
		return name;
	}

	void loadSideBarConfig() {
		static QMap<const char*, bool> defaultVisibility;
		if (defaultVisibility.isEmpty()) {
			defaultVisibility[BROWSE_MODE_SIDE_BAR_GROUP]     = true;
			defaultVisibility[VIEW_MODE_SIDE_BAR_GROUP]       = true;
			defaultVisibility[FULLSCREEN_MODE_SIDE_BAR_GROUP] = false;
		}

		const char* name = sideBarConfigGroupName();
		KConfigGroup group(KGlobal::config(), name);
		mSideBar->setVisible(group.readEntry(SIDE_BAR_IS_VISIBLE_KEY, defaultVisibility[name]));
		mSideBar->setCurrentPage(GwenviewConfig::sideBarPage());
	}

	void saveSideBarConfig() const {
		KConfigGroup group(KGlobal::config(), sideBarConfigGroupName());
		group.writeEntry(SIDE_BAR_IS_VISIBLE_KEY, mSideBar->isVisible());
		GwenviewConfig::setSideBarPage(mSideBar->currentPage());
	}

	void setScreenSaverEnabled(bool enabled) {
		// Always delete mNotificationRestrictions, it does not hurt
		delete mNotificationRestrictions;
		if (!enabled) {
			mNotificationRestrictions = new KNotificationRestrictions(KNotificationRestrictions::ScreenSaver, mWindow);
		} else {
			mNotificationRestrictions = 0;
		}
	}
};


MainWindow::MainWindow()
: KXmlGuiWindow(),
d(new MainWindow::Private)
{
	d->mWindow = this;
	d->mCurrentPageId = StartPageId;
	d->mDistractionFreeMode = false;
	d->mDirModel = new SortedDirModel(this);
	d->mGvCore = new GvCore(this, d->mDirModel);
	d->mPreloader = new Preloader(this);
	d->mNotificationRestrictions = 0;
	d->initDirModel();
	d->setupWidgets();
	d->setupActions();
	d->setupUndoActions();
	d->setupContextManager();
	d->setupFullScreenBar();
	d->updateActions();
	updatePreviousNextActions();
	d->mSaveBar->initActionDependentWidgets();

	createGUI();
	loadConfig();

	connect(DocumentFactory::instance(), SIGNAL(modifiedDocumentListChanged()),
		SLOT(slotModifiedDocumentListChanged()) );

#ifdef KIPI_FOUND
	d->mKIPIInterface = new KIPIInterface(this);
	d->mKIPIExportAction->setKIPIInterface(d->mKIPIInterface);
#endif
	setAutoSaveSettings();
}


MainWindow::~MainWindow() {
	delete d;
}

ContextManager* MainWindow::contextManager() const {
	return d->mContextManager;
}

DocumentPanel* MainWindow::documentPanel() const {
	return d->mDocumentPanel;
}


bool MainWindow::currentDocumentIsRasterImage() const {
	if (d->mCurrentPageId == ViewPageId) {
		Document::Ptr doc = d->mDocumentPanel->currentDocument();
		if (!doc) {
			return false;
		}
		return doc->kind() == MimeTypeUtils::KIND_RASTER_IMAGE;
	} else {
		QModelIndex index = d->mThumbnailView->currentIndex();
		if (!index.isValid()) {
			return false;
		}
		KFileItem item = d->mDirModel->itemForIndex(index);
		Q_ASSERT(!item.isNull());
		return MimeTypeUtils::fileItemKind(item) == MimeTypeUtils::KIND_RASTER_IMAGE;
	}
}


void MainWindow::setCaption(const QString& caption) {
	// Keep a trace of caption to use it in slotModifiedDocumentListChanged()
	d->mCaption = caption;
	KXmlGuiWindow::setCaption(caption);
}

void MainWindow::setCaption(const QString& caption, bool modified) {
	d->mCaption = caption;
	KXmlGuiWindow::setCaption(caption, modified);
}


void MainWindow::slotModifiedDocumentListChanged() {
	d->updateActions();

	// Update caption
	QList<KUrl> list = DocumentFactory::instance()->modifiedDocumentList();
	bool modified = list.count() > 0;
	setCaption(d->mCaption, modified);
}


void MainWindow::setInitialUrl(const KUrl& _url) {
	Q_ASSERT(_url.isValid());
	KUrl url = UrlUtils::fixUserEnteredUrl(_url);
	if (url.protocol() == "http" || url.protocol() == "https") {
		d->mGvCore->addUrlToRecentUrls(url);
	}
	if (UrlUtils::urlIsDirectory(url)) {
		d->mBrowseAction->trigger();
		openDirUrl(url);
	} else {
		d->mViewAction->trigger();
		d->mDocumentPanel->openUrl(url);
		d->setUrlToSelect(url);
	}
	d->updateContextDependentComponents();
}


void MainWindow::startSlideShow() {
	// We need to wait until we have listed all images in the dirlister to
	// start the slideshow because the SlideShow objects needs an image list to
	// work.
	d->mStartSlideShowWhenDirListerCompleted = true;
}


void MainWindow::setActiveViewModeAction(QAction* action) {
	if (d->mCurrentPageId == StartPageId) {
		d->mSideBarCollapser->show();
	} else {
		d->saveSideBarConfig();
	}
	if (action == d->mViewAction) {
		d->mCurrentPageId = ViewPageId;
		// Switching to view mode
		d->setDirModelShowDirs(false);
		d->mViewStackedWidget->setCurrentWidget(d->mDocumentPanel);
		d->mContextManager->setOnlyCurrentUrl(true);
		if (d->mDocumentPanel->isEmpty()) {
			openSelectedDocuments();
		}
	} else {
		d->mCurrentPageId = BrowsePageId;
		// Switching to browse mode
		d->mViewStackedWidget->setCurrentWidget(d->mThumbnailViewPanel);
		if (!d->mDocumentPanel->isEmpty()
			&& KProtocolManager::supportsListing(d->mDocumentPanel->url()))
		{
			// Reset the view to spare resources, but don't do it if we can't
			// browse the url, otherwise if the user starts Gwenview this way:
			// gwenview http://example.com/example.png
			// and switch to browse mode, switching back to view mode won't bring
			// his image back.
			d->mDocumentPanel->reset();
		}
		d->setDirModelShowDirs(true);
		setCaption(QString());
		d->mContextManager->setOnlyCurrentUrl(false);
	}
	d->loadSideBarConfig();

	emit viewModeChanged();
}


void MainWindow::slotThumbnailViewIndexActivated(const QModelIndex& index) {
	if (!index.isValid()) {
		return;
	}

	KFileItem item = d->mDirModel->itemForIndex(index);
	if (item.isDir()) {
		// Item is a dir, open it
		openDirUrl(item.url());
	} else {
		QString protocol = ArchiveUtils::protocolForMimeType(item.mimetype());
		if (!protocol.isEmpty()) {
			// Item is an archive, tweak url then open it
			KUrl url = item.url();
			url.setProtocol(protocol);
			openDirUrl(url);
		} else {
			// Item is a document, switch to view mode
			d->mViewAction->trigger();
		}
	}
}


void MainWindow::openSelectedDocuments() {
	if (d->mCurrentPageId != ViewPageId) {
		return;
	}

	QModelIndex currentIndex = d->mThumbnailView->currentIndex();
	if (!currentIndex.isValid()) {
		return;
	}

	int count = 0;

	KUrl::List urls;
	KUrl currentUrl;
	Q_FOREACH(const QModelIndex& index, d->mThumbnailView->selectionModel()->selectedIndexes()) {
		KFileItem item = d->mDirModel->itemForIndex(index);
		if (!item.isNull() && !ArchiveUtils::fileItemIsDirOrArchive(item)) {
			KUrl url = item.url();
			urls << url;
			if (index == currentIndex) {
				currentUrl = url;
			}
			++count;
			if (count == DocumentPanel::MaxViewCount) {
				break;
			}
		}
	}

	d->mDocumentPanel->openUrls(urls, currentUrl);
}


void MainWindow::goUp() {
	if (d->mCurrentPageId == BrowsePageId) {
		KUrl url = d->mDirModel->dirLister()->url();
		url = url.upUrl();
		openDirUrl(url);
	} else {
		d->mBrowseAction->trigger();
	}
}


void MainWindow::showStartPage() {
	if (d->mCurrentPageId != StartPageId) {
		d->saveSideBarConfig();
		d->mCurrentPageId = StartPageId;
	}
	d->setActionsDisabledOnStartPageEnabled(false);
	d->spreadCurrentDirUrl(KUrl());

	d->mSideBar->hide();
	d->mSideBarCollapser->hide();
	d->mViewStackedWidget->setCurrentWidget(d->mStartPage);

	d->updateActions();
	updatePreviousNextActions();
	d->updateContextDependentComponents();
}


void MainWindow::slotStartPageUrlSelected(const KUrl& url) {
	d->setActionsDisabledOnStartPageEnabled(true);

	if (d->mBrowseAction->isChecked()) {
		// Silently uncheck the action so that setInitialUrl() does the right thing
		SignalBlocker blocker(d->mBrowseAction);
		d->mBrowseAction->setChecked(false);
	}

	setInitialUrl(url);
}


void MainWindow::openDirUrl(const KUrl& url) {
	const KUrl currentUrl = d->mContextManager->currentDirUrl();

	if (url.equals(currentUrl, KUrl::CompareWithoutTrailingSlash)) {
		return;
	}

	if (url.isParentOf(currentUrl)) {
		// Keep first child between url and currentUrl selected
		// If currentPath is      "/home/user/photos/2008/event"
		// and wantedPath is      "/home/user/photos"
		// pathToSelect should be "/home/user/photos/2008"
		const QString currentPath = QDir::cleanPath(currentUrl.toLocalFile(KUrl::RemoveTrailingSlash));
		const QString wantedPath  = QDir::cleanPath(url.toLocalFile(KUrl::RemoveTrailingSlash));
		const QChar separator('/');
		const int slashCount = wantedPath.count(separator);
		const QString pathToSelect = currentPath.section(separator, 0, slashCount + 1);
		KUrl urlToSelect = url;
		urlToSelect.setPath(pathToSelect);
		d->setUrlToSelect(urlToSelect);
	}
	d->mDirModel->dirLister()->openUrl(url);
	d->spreadCurrentDirUrl(url);
	d->mDocumentPanel->reset();
}


void MainWindow::toggleSideBar(bool on) {
	d->mSideBar->setVisible(on);
}


void MainWindow::updateToggleSideBarAction() {
	SignalBlocker blocker(d->mToggleSideBarAction);
	d->mToggleSideBarAction->setChecked(d->mSideBar->isVisible());
}


void MainWindow::slotPartCompleted() {
	d->updateActions();
	KUrl url = d->mDocumentPanel->url();
	if (!KProtocolManager::supportsListing(url)) {
		return;
	}

	KUrl dirUrl = url;
	dirUrl.setFileName(QString());
	if (dirUrl.equals(d->mDirModel->dirLister()->url(), KUrl::CompareWithoutTrailingSlash)) {
		QModelIndex index = d->mDirModel->indexForUrl(url);
		QItemSelectionModel* selectionModel = d->mThumbnailView->selectionModel();
		if (index.isValid() && !selectionModel->isSelected(index)) {
			selectionModel->select(index, QItemSelectionModel::SelectCurrent);
		}
	} else {
		d->mDirModel->dirLister()->openUrl(dirUrl);
		d->spreadCurrentDirUrl(dirUrl);
	}
}


void MainWindow::slotSelectionChanged() {
	if (d->mCurrentPageId == ViewPageId) {
		// The user selected a new file in the thumbnail view, since the
		// document view is visible, let's show it
		openSelectedDocuments();
	} else {
		// No document view, we need to load the document to set the undo group
		// of document factory to the correct QUndoStack
		QModelIndex index = d->mThumbnailView->currentIndex();
		KFileItem item;
		if (index.isValid()) {
			item = d->mDirModel->itemForIndex(index);
		}
		QUndoGroup* undoGroup = DocumentFactory::instance()->undoGroup();
		if (!item.isNull() && !ArchiveUtils::fileItemIsDirOrArchive(item)) {
			KUrl url = item.url();
			Document::Ptr doc = DocumentFactory::instance()->load(url);
			undoGroup->addStack(doc->undoStack());
			undoGroup->setActiveStack(doc->undoStack());
		} else {
			undoGroup->setActiveStack(0);
		}
	}

	// Update UI
	d->updateActions();
	updatePreviousNextActions();
	d->updateContextDependentComponents();

	// Start preloading
	QTimer::singleShot(PRELOAD_DELAY, this, SLOT(preloadNextUrl()) );
}


void MainWindow::slotDirModelNewItems() {
	if (d->mUrlToSelect.isValid()) {
		d->selectUrlToSelect();
	}
	if (d->mThumbnailView->selectionModel()->hasSelection()) {
		updatePreviousNextActions();
	}
}


void MainWindow::slotDirListerCompleted() {
	if (d->mStartSlideShowWhenDirListerCompleted) {
		d->mStartSlideShowWhenDirListerCompleted = false;
		QTimer::singleShot(0, d->mToggleSlideShowAction, SLOT(trigger()));
	}
	if (d->mThumbnailView->selectionModel()->hasSelection()) {
		updatePreviousNextActions();
	} else if (!d->mUrlToSelect.isValid()) {
		d->goToFirstDocument();
	}
}


void MainWindow::goToPrevious() {
	d->goTo(-1);
}


void MainWindow::goToNext() {
	d->goTo(1);
}

void MainWindow::goToFirst() {
	d->goToFirstDocument();
}

void MainWindow::goToLast() {
	d->goToLastDocument();
}

void MainWindow::goToUrl(const KUrl& url) {
	if (d->mCurrentPageId == ViewPageId) {
		d->mDocumentPanel->openUrl(url);
	}
	KUrl dirUrl = url;
	dirUrl.setFileName("");
	if (!dirUrl.equals(d->mContextManager->currentDirUrl(), KUrl::CompareWithoutTrailingSlash)) {
		d->mDirModel->dirLister()->openUrl(dirUrl);
		d->spreadCurrentDirUrl(dirUrl);
	}
	d->setUrlToSelect(url);
}


void MainWindow::updatePreviousNextActions() {
	QModelIndex index = d->currentIndex();

	QModelIndex prevIndex = d->mDirModel->index(index.row() - 1, 0);
	d->mGoToPreviousAction->setEnabled(prevIndex.isValid() && !d->indexIsDirOrArchive(prevIndex));
	d->mGoToFirstAction->setEnabled(d->mGoToPreviousAction->isEnabled());

	QModelIndex nextIndex = d->mDirModel->index(index.row() + 1, 0);
	d->mGoToNextAction->setEnabled(nextIndex.isValid() && !d->indexIsDirOrArchive(nextIndex));
	d->mGoToLastAction->setEnabled(d->mGoToNextAction->isEnabled());
}


void MainWindow::leaveFullScreen() {
	if (d->mFullScreenAction->isChecked()) {
		d->mFullScreenAction->trigger();
	}
}


void MainWindow::toggleFullScreen(bool checked) {
	setUpdatesEnabled(false);
	d->saveSideBarConfig();
	if (checked) {
		// Save MainWindow config now, this way if we quit while in
		// fullscreen, we are sure latest MainWindow changes are remembered.
		saveMainWindowSettings(autoSaveConfigGroup());
		resetAutoSaveSettings();

		// Save state
		d->mStateBeforeFullScreen.mToolBarVisible = toolBar()->isVisible();
		d->mStateBeforeFullScreen.mWindowState = windowState();

		// Go full screen
		setWindowState(windowState() | Qt::WindowFullScreen);
		menuBar()->hide();
		toolBar()->hide();

		QApplication::setPalette(d->mFullScreenPalette);
		d->mThumbnailViewPanel->setFullScreenMode(true);
		d->mDocumentPanel->setFullScreenMode(true);
		d->mSaveBar->setFullScreenMode(true);
		d->updateDistractionsState();
		d->setScreenSaverEnabled(false);

		// HACK: Only load sidebar config now, because it looks at
		// DocumentPanel fullScreenMode property to determine the sidebar
		// config group.
		d->loadSideBarConfig();
	} else {
		setAutoSaveSettings();

		// Back to normal
		QApplication::setPalette(KGlobalSettings::createApplicationPalette());
		d->mThumbnailViewPanel->setFullScreenMode(false);
		d->mDocumentPanel->setFullScreenMode(false);
		d->mSlideShow->stop();
		d->mSaveBar->setFullScreenMode(false);
		setWindowState(d->mStateBeforeFullScreen.mWindowState);
		menuBar()->setVisible(d->mShowMenuBarAction->isChecked());
		toolBar()->setVisible(d->mStateBeforeFullScreen.mToolBarVisible);

		d->updateDistractionsState();
		d->setScreenSaverEnabled(true);

		// Keep this after mDocumentPanel->setFullScreenMode(false).
		// See call to loadSideBarConfig() above.
		d->loadSideBarConfig();

		// See resizeEvent
		d->mFullScreenLeftAt = QDateTime::currentDateTime();
	}
	setUpdatesEnabled(true);
}


void MainWindow::saveCurrent() {
	d->mGvCore->save(d->currentUrl());
}


void MainWindow::saveCurrentAs() {
	d->mGvCore->saveAs(d->currentUrl());
}


void MainWindow::reload() {
	if (d->mCurrentPageId == ViewPageId) {
		d->mDocumentPanel->reload();
	} else {
		d->mThumbnailViewPanel->reload();
	}
}


void MainWindow::openFile() {
	KUrl dirUrl = d->mDirModel->dirLister()->url();

	KFileDialog dialog(dirUrl, QString(), this);
	dialog.setCaption(i18nc("@title:window", "Open Image"));
	const QStringList mimeFilter = MimeTypeUtils::imageMimeTypes();
	dialog.setMimeFilter(mimeFilter);
	dialog.setOperationMode(KFileDialog::Opening);
	if (!dialog.exec()) {
		return;
	}

	d->setActionsDisabledOnStartPageEnabled(true);
	KUrl url = dialog.selectedUrl();
	d->mViewAction->trigger();
	d->mDocumentPanel->openUrl(url);
	d->setUrlToSelect(url);
	d->updateContextDependentComponents();
}


void MainWindow::showDocumentInFullScreen(const KUrl& url) {
	d->mDocumentPanel->openUrl(url);
	d->setUrlToSelect(url);
	d->mFullScreenAction->trigger();
}


void MainWindow::toggleSlideShow() {
	if (d->mSlideShow->isRunning()) {
		d->mSlideShow->stop();
	} else {
		if (!d->mFullScreenAction->isChecked()) {
			d->mFullScreenAction->trigger();
		}
		QList<KUrl> list;
		for (int pos=0; pos < d->mDirModel->rowCount(); ++pos) {
			QModelIndex index = d->mDirModel->index(pos, 0);
			KFileItem item = d->mDirModel->itemForIndex(index);
			MimeTypeUtils::Kind kind = MimeTypeUtils::fileItemKind(item);
			switch (kind) {
			case MimeTypeUtils::KIND_SVG_IMAGE:
			case MimeTypeUtils::KIND_RASTER_IMAGE:
			case MimeTypeUtils::KIND_VIDEO:
				list << item.url();
				break;
			default:
				break;
			}
		}
		d->mSlideShow->start(list);
	}
	updateSlideShowAction();
}


void MainWindow::updateSlideShowAction() {
	if (d->mSlideShow->isRunning()) {
		d->mToggleSlideShowAction->setText(i18n("Stop Slideshow"));
		d->mToggleSlideShowAction->setIcon(KIcon("media-playback-pause"));
	} else {
		d->mToggleSlideShowAction->setText(i18n("Start Slideshow"));
		d->mToggleSlideShowAction->setIcon(KIcon("media-playback-start"));
	}
}


bool MainWindow::queryClose() {
	saveConfig();
	d->saveSideBarConfig();
	QList<KUrl> list = DocumentFactory::instance()->modifiedDocumentList();
	if (list.size() == 0) {
		return true;
	}

	KGuiItem yes(i18n("Save All Changes"), "document-save-all");
	KGuiItem no(i18n("Discard Changes"));
	QString msg = i18np("One image has been modified.", "%1 images have been modified.", list.size())
			+ '\n'
			+ i18n("If you quit now, your changes will be lost.");
	int answer = KMessageBox::warningYesNoCancel(
		this,
		msg,
		QString() /* caption */,
		yes,
		no);

	switch (answer) {
	case KMessageBox::Yes:
		d->mGvCore->saveAll();
		// We need to wait a bit because the DocumentFactory is notified about
		// saved documents through a queued connection.
		qApp->processEvents();
		return DocumentFactory::instance()->modifiedDocumentList().isEmpty();

	case KMessageBox::No:
		return true;

	default: // cancel
		return false;
	}
}


bool MainWindow::queryExit() {
	if (GwenviewConfig::deleteThumbnailCacheOnExit()) {
		const QString dir = ThumbnailLoadJob::thumbnailBaseDir();
		if (QFile::exists(dir)) {
			KIO::NetAccess::del(KUrl::fromPath(dir), this);
		}
	}
	return true;
}


void MainWindow::showConfigDialog() {
	ConfigDialog dialog(this);
	connect(&dialog, SIGNAL(settingsChanged(QString)), SLOT(loadConfig()));
	dialog.exec();
}


void MainWindow::toggleMenuBar() {
	if (!d->mFullScreenAction->isChecked()) {
		menuBar()->setVisible(d->mShowMenuBarAction->isChecked());
	}
}


void MainWindow::loadConfig() {
	d->mDirModel->setBlackListedExtensions(GwenviewConfig::blackListedExtensions());
	MimeTypeUtils::Kinds kind = d->mDirModel->kindFilter();
	d->mDirModel->adjustKindFilter(MimeTypeUtils::KIND_VIDEO, GwenviewConfig::listVideos());

	d->mDocumentPanel->loadConfig();
	d->mThumbnailViewPanel->loadConfig();

	// Colors
	int value = GwenviewConfig::viewBackgroundValue();
	QColor bgColor = QColor::fromHsv(0, 0, value);
	QColor fgColor = value > 128 ? Qt::black : Qt::white;

	QPalette normalPal = palette();
	normalPal.setColor(QPalette::Base, bgColor);
	normalPal.setColor(QPalette::Text, fgColor);

	// Apply to widgets
	d->mStartPage->applyPalette(normalPal);
	d->mThumbnailViewPanel->setNormalPalette(normalPal);
	d->mDocumentPanel->setNormalPalette(normalPal);

	// FIXME: Should we avoid CSS here to get a more native look?
	d->mSideBarCollapser->setAutoFillBackground(true);
	d->mSideBarCollapser->setStyleSheet(
		"	background-color: palette(window);"
		"	border: 1px solid palette(mid);"
		"	border-radius: 5px;"
		);
}


void MainWindow::saveConfig() {
	d->mDocumentPanel->saveConfig();
	d->mThumbnailViewPanel->saveConfig();
}


void MainWindow::print() {
	if (!currentDocumentIsRasterImage()) {
		return;
	}

	Document::Ptr doc = DocumentFactory::instance()->load(d->currentUrl());
	PrintHelper printHelper(this);
	printHelper.print(doc);
}


void MainWindow::preloadNextUrl() {
	static bool disablePreload = qgetenv("GWENVIEW_MAX_UNREFERENCED_IMAGES") == "0";
	if (disablePreload) {
		kDebug() << "Preloading disabled";
		return;
	}
	QItemSelection selection = d->mThumbnailView->selectionModel()->selection();
	if (selection.size() != 1) {
		return;
	}

	QModelIndex index = selection.indexes()[0];
	if (!index.isValid()) {
		return;
	}

	if (d->mCurrentPageId == ViewPageId) {
		// If we are in view mode, preload the next url, otherwise preload the
		// selected one
		index = d->mDirModel->sibling(index.row() + 1, index.column(), index);
		if (!index.isValid()) {
			return;
		}
	}

	KFileItem item = d->mDirModel->itemForIndex(index);
	if (!ArchiveUtils::fileItemIsDirOrArchive(item)) {
		KUrl url = item.url();
		if (url.isLocalFile()) {
			QSize size = d->mViewStackedWidget->size();
			d->mPreloader->preload(url, size);
		}
	}
}


QSize MainWindow::sizeHint() const {
	return KXmlGuiWindow::sizeHint().expandedTo(QSize(750, 500));
}


void MainWindow::showEvent(QShowEvent *event) {
	// We need to delay initializing the action state until the menu bar has
	// been initialized, that's why it's done only in the showEvent()
	d->mShowMenuBarAction->setChecked(menuBar()->isVisible());
	KXmlGuiWindow::showEvent(event);
}


void MainWindow::resizeEvent(QResizeEvent* event) {
	KXmlGuiWindow::resizeEvent(event);
	// This is a small hack to execute code after leaving fullscreen, and after
	// the window has been resized back to its former size.
	if (d->mFullScreenLeftAt.isValid() && d->mFullScreenLeftAt.secsTo(QDateTime::currentDateTime()) < 2) {
		if (d->mCurrentPageId == BrowsePageId) {
			d->mThumbnailView->scrollToSelectedIndex();
		}
		d->mFullScreenLeftAt = QDateTime();
	}
}


void MainWindow::setDistractionFreeMode(bool value) {
	d->mDistractionFreeMode = value;
	d->updateDistractionsState();
}


void MainWindow::showMessageBubble(MessageBubble* bubble) {
	WidgetFloater* floater = new WidgetFloater(d->mContentWidget);
	floater->setChildWidget(bubble);
	floater->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
	bubble->show();
}


void MainWindow::saveProperties(KConfigGroup& group) {
	group.writeEntry(SESSION_CURRENT_PAGE_KEY, int(d->mCurrentPageId));
	group.writeEntry(SESSION_URL_KEY, d->currentUrl());
}


void MainWindow::readProperties(const KConfigGroup& group) {
	PageId pageId = PageId(group.readEntry(SESSION_CURRENT_PAGE_KEY, int(StartPageId)));
	if (pageId == StartPageId) {
		d->mCurrentPageId = StartPageId;
		showStartPage();
	} else if (pageId == BrowsePageId) {
		d->mBrowseAction->trigger();
	} else {
		d->mViewAction->trigger();
	}
	KUrl url = group.readEntry(SESSION_URL_KEY, KUrl());
	if (!url.isValid()) {
		kWarning() << "Invalid url!";
		return;
	}
	goToUrl(url);
}


} // namespace
