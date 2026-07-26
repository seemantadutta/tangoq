#include "widget/wtrackmenu.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QInputDialog>
#include <QList>
#include <QListWidget>
#include <QMessageBox>
#include <QModelIndex>
#include <QVBoxLayout>

#include "analyzer/analyzerscheduledtrack.h"
#include "analyzer/analyzersilence.h"
#include "analyzer/analyzertrack.h"
#include "control/controlobject.h"
#include "library/autodj/cortinaregistry.h"
#include "library/basetracktablemodel.h"
#include "library/coverartutils.h"
#include "library/dao/trackschema.h"
#include "library/dlgtagfetcher.h"
#include "library/dlgtrackinfo.h"
#include "library/dlgtrackinfomulti.h"
#include "library/dlgtrackmetadataexport.h"
#include "library/externaltrackcollection.h"
#include "library/library.h"
#include "library/trackcollection.h"
#include "library/trackcollectionmanager.h"
#include "library/trackmodel.h"
#include "library/trackmodeliterator.h"
#include "library/trackprocessing.h"
#include "library/trackset/crate/crate.h"
#include "library/trackset/crate/cratefeaturehelper.h"
#include "library/trackset/crate/cratesummary.h"
#include "mixer/playerinfo.h"
#include "mixer/playermanager.h"
#include "moc_wtrackmenu.cpp"
#include "preferences/colorpalettesettings.h"
#include "preferences/configobject.h"
#include "preferences/dialog/dlgprefdeck.h"
#include "sources/soundsourceproxy.h"
#include "track/track.h"
#include "util/defs.h"
#include "util/desktophelper.h"
#include "util/duration.h"
#include "util/parented_ptr.h"
#include "util/qt.h"
#include "util/widgethelper.h"
#include "widget/findonwebmenufactory.h"
#include "widget/wcolorpickeraction.h"
#include "widget/wcoverartlabel.h"
#include "widget/wcoverartmenu.h"
#include "widget/wfindonwebmenu.h"
#include "widget/wmenucheckbox.h"
#include "widget/wsearchrelatedtracksmenu.h"
// WStarRating is required for DlgTrackInfo
#include "widget/wstarrating.h"
#include "widget/wstarratingaction.h"

constexpr WTrackMenu::Features WTrackMenu::kDeckTrackMenuFeatures;

namespace {
const QString kAppGroup = QStringLiteral("[App]");

const QString samplerTrString(int i) {
    return QObject::tr("Sampler %1").arg(i);
}

const char* kOrigTrTextProperty = "origTrText";
const char* kBpmScaleProperty = "bpmScale";

void appendBpmPreviewtoBpmAction(QAction* pAction, const double bpm) {
    QString text = pAction->property(kOrigTrTextProperty).toString();
    bool ok = false;
    const double scale = pAction->property(kBpmScaleProperty).toDouble(&ok);
    if (ok && !text.isEmpty()) {
        QString scaledBpm = QString::number(bpm * scale, 'f', 2);
        // This is claimed to be the most performant way to get
        // * 2 decimals for floating point values and
        // * no trailing zero or dot for even numbers
        // https://stackoverflow.com/a/65789182
        while (scaledBpm.back() == '0') {
            scaledBpm.chop(1);
        }
        if (scaledBpm.back() == '.') {
            scaledBpm.chop(1);
        }
        text.append(QStringLiteral(" | %1 BPM").arg(scaledBpm));
        pAction->setText(text);
    }
}

void storeActionTextAndScaleInProperties(QAction* pAction, const double scale) {
    VERIFY_OR_DEBUG_ASSERT(pAction && scale != 0.0) {
        return;
    }
    pAction->setProperty(kOrigTrTextProperty, QVariant::fromValue(pAction->text()));
    pAction->setProperty(kBpmScaleProperty, QVariant(scale));
}

// Sums the duration of the given rows by reading the model's Duration column,
// the same cheap approach as PlaylistTableModel::getTotalDuration (no per-track
// database access). Returns empty for a non-tabular model or empty selection.
mixxx::Duration sumTracksDuration(
        TrackModel* pTrackModel, const QModelIndexList& indices) {
    const auto* pTableModel = dynamic_cast<const BaseTrackTableModel*>(pTrackModel);
    if (!pTableModel || indices.isEmpty()) {
        return mixxx::Duration::empty();
    }
    const int durationColumn =
            pTableModel->fieldIndex(ColumnCache::COLUMN_LIBRARYTABLE_DURATION);
    if (durationColumn < 0) {
        return mixxx::Duration::empty();
    }
    double totalSeconds = 0.0;
    for (const auto& index : indices) {
        totalSeconds += index.sibling(index.row(), durationColumn)
                                .data(Qt::EditRole)
                                .toDouble();
    }
    return mixxx::Duration::fromSeconds(totalSeconds);
}

} // namespace

WTrackMenu::WTrackMenu(
        QWidget* parent,
        UserSettingsPointer pConfig,
        Library* pLibrary,
        Features flags,
        TrackModel* trackModel)
        : QMenu(parent),
          m_pTrackModel(trackModel),
          m_pConfig(pConfig),
          m_pLibrary(pLibrary),
          m_pNumSamplers(kAppGroup, QStringLiteral("num_samplers")),
          m_pNumDecks(kAppGroup, QStringLiteral("num_decks")),
          m_pNumPreviewDecks(kAppGroup, QStringLiteral("num_preview_decks")),
          m_bPlaylistMenuLoaded(false),
          m_bCrateMenuLoaded(false),
          m_eActiveFeatures(flags),
          m_eTrackModelFeatures(Feature::TrackModelFeatures) {
    // Warn if any of the chosen features depend on a TrackModel
    VERIFY_OR_DEBUG_ASSERT(trackModel || (m_eTrackModelFeatures & flags) == 0) {
        // Remove unsupported features
        m_eActiveFeatures &= !m_eTrackModelFeatures;
    }

    createMenus();
    createActions();
    setupActions();
}

WTrackMenu::~WTrackMenu() {
    // ~QPointer() needs the definition of the wrapped type
    // upon deletion! Otherwise the behavior is undefined.
    // The wrapped types of some QPointer members are only
    // forward declared in the header file.
}

int WTrackMenu::getTrackCount() const {
    if (m_pTrackModel) {
        return m_trackIndexList.size();
    } else {
        return m_pTrack ? 1 : 0;
    }
}

const QString WTrackMenu::getDeckGroup() const {
    return m_deckGroup;
}

void WTrackMenu::closeEvent(QCloseEvent* event) {
    // Unfortunately, trackMenuVisible(false) is emitted before the menu is effectively
    // closed, which causes issues in WTrackProperty::slotShowTrackMenuChangeRequest.
    // Explicitly hide() to avoid this.
    hide();
    // Actually the event is accepted by default. doing it explicitly doesn't hurt.
    // If it's not accepted the menu remains open and entire GUI will be blocked!
    event->accept();
    emit trackMenuVisible(false);
}

void WTrackMenu::popup(const QPoint& pos, QAction* at) {
    if (isEmpty()) {
        return;
    }
    QMenu::popup(pos, at);
    emit trackMenuVisible(true);
}

void WTrackMenu::createMenus() {
    if (featureIsEnabled(Feature::LoadTo)) {
        m_pLoadToMenu = make_parented<QMenu>(this);
        m_pLoadToMenu->setTitle(tr("Load to"));
        m_pDeckMenu = make_parented<QMenu>(m_pLoadToMenu);
        m_pDeckMenu->setTitle(tr("Deck"));
        m_pSamplerMenu = make_parented<QMenu>(m_pLoadToMenu);
        m_pSamplerMenu->setTitle(tr("Sampler"));
    }

    if (featureIsEnabled(Feature::Playlist)) {
        m_pPlaylistMenu = make_parented<QMenu>(this);
        m_pPlaylistMenu->setTitle(tr("Add to Playlist"));
        connect(m_pPlaylistMenu, &QMenu::aboutToShow, this, &WTrackMenu::slotPopulatePlaylistMenu);
    }

    if (featureIsEnabled(Feature::Crate)) {
        m_pCrateMenu = make_parented<QMenu>(this);
        m_pCrateMenu->setTitle(tr("Crates"));
        m_pCrateMenu->setObjectName("CratesMenu");
        connect(m_pCrateMenu, &QMenu::aboutToShow, this, &WTrackMenu::slotPopulateCrateMenu);
    }

    if (featureIsEnabled(Feature::Metadata)) {
        m_pMetadataMenu = make_parented<QMenu>(this);
        m_pMetadataMenu->setTitle(tr("Metadata"));

        m_pMetadataUpdateExternalCollectionsMenu = make_parented<QMenu>(m_pMetadataMenu);
        m_pMetadataUpdateExternalCollectionsMenu->setTitle(tr("Update external collections"));

        m_pCoverMenu = make_parented<WCoverArtMenu>(m_pMetadataMenu);
        m_pCoverMenu->setTitle(tr("Cover Art"));
        connect(m_pCoverMenu.get(),
                &WCoverArtMenu::coverInfoSelected,
                this,
                &WTrackMenu::slotCoverInfoSelected);
        connect(m_pCoverMenu.get(),
                &WCoverArtMenu::reloadCoverArt,
                this,
                &WTrackMenu::slotReloadCoverArt);
    }

    if (featureIsEnabled(Feature::BPM)) {
        m_pBPMMenu = make_parented<QMenu>(this);
        m_pBPMMenu->setTitle(tr("Adjust BPM"));
    }

    if (featureIsEnabled(Feature::Color)) {
        m_pColorMenu = make_parented<QMenu>(this);
        m_pColorMenu->setTitle(tr("Select Color"));
    }

    if (featureIsEnabled(Feature::Reset)) {
        m_pClearMetadataMenu = make_parented<QMenu>(this);
        //: Reset metadata in right click track context menu in library
        m_pClearMetadataMenu->setTitle(tr("Clear"));
    }

    if (featureIsEnabled(Feature::Analyze)) {
        m_pAnalyzeMenu = make_parented<QMenu>(this);
        m_pAnalyzeMenu->setTitle(tr("Analyze"));
    }

    if (featureIsEnabled(Feature::SearchRelated)) {
        DEBUG_ASSERT(!m_pSearchRelatedMenu);
        m_pSearchRelatedMenu =
                make_parented<WSearchRelatedTracksMenu>(this);
        connect(m_pSearchRelatedMenu,
                &QMenu::aboutToShow,
                this,
                [this] {
                    // TODO When accidentally leaving the menu and reopening it,
                    // the previous check states are cleared.
                    // Clear in closeEvent() only? And create actions on aboutToShow
                    // only if it's empty?
                    m_pSearchRelatedMenu->clear();
                    const auto pTrack = getFirstTrackPointer();
                    if (pTrack) {
                        // Ensure it's enabled, else we can't add actions.
                        VERIFY_OR_DEBUG_ASSERT(m_pSearchRelatedMenu->isEnabled()) {
                            m_pSearchRelatedMenu->setEnabled(true);
                        }
                        m_pSearchRelatedMenu->addActionsForTrack(*pTrack);
                    }
                    m_pSearchRelatedMenu->setEnabled(
                            !m_pSearchRelatedMenu->isEmpty());
                });
        connect(m_pSearchRelatedMenu,
                &WSearchRelatedTracksMenu::triggerSearch,
                this,
                [this](const QString& searchQuery) {
                    m_pLibrary->searchTracksInCollection(searchQuery);
                    hide();
                });
    }

    if (featureIsEnabled(Feature::FindOnWeb)) {
        DEBUG_ASSERT(!m_pFindOnWebMenu);
        m_pFindOnWebMenu = make_parented<WFindOnWebMenu>(this);
        connect(m_pFindOnWebMenu,
                &QMenu::aboutToShow,
                this,
                [this] {
                    m_pFindOnWebMenu->clear();
                    const auto pTrack = getFirstTrackPointer();
                    if (pTrack) {
                        mixxx::library::createFindOnWebSubmenus(
                                m_pFindOnWebMenu,
                                *pTrack);
                    }
                    m_pFindOnWebMenu->setEnabled(
                            !m_pFindOnWebMenu->isEmpty());
                });
    }

    if (featureIsEnabled(Feature::RemoveFromDisk)) {
        // Qt added QFile::MoveToTrash() in 5.15. If that's not available we
        // permanently delete files, put the action into a submenu for safety
        // reasons and display different messages in the delete dialogs.
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
        m_pRemoveFromDiskMenu = make_parented<QMenu>(this);
        m_pRemoveFromDiskMenu->setTitle(tr("Delete Track Files"));
#endif
    }
}

void WTrackMenu::createActions() {
    const auto hideRemoveKeySequence =
            // TODO(XXX): Qt6 replace enum | with QKeyCombination
            QKeySequence(static_cast<int>(kHideRemoveShortcutModifier) |
                    kHideRemoveShortcutKey);

    if (featureIsEnabled(Feature::AutoDJ)) {
        m_pAutoDJBottomAct = make_parented<QAction>(tr("Add to Auto DJ Queue (bottom)"), this);
        connect(m_pAutoDJBottomAct, &QAction::triggered, this, &WTrackMenu::slotAddToAutoDJBottom);

        m_pAutoDJTopAct = make_parented<QAction>(tr("Add to Auto DJ Queue (top)"), this);
        connect(m_pAutoDJTopAct, &QAction::triggered, this, &WTrackMenu::slotAddToAutoDJTop);

        m_pAutoDJReplaceAct = make_parented<QAction>(tr("Add to Auto DJ Queue (replace)"), this);
        connect(m_pAutoDJReplaceAct, &QAction::triggered, this, &WTrackMenu::slotAddToAutoDJReplace);

        m_pAutoDJCortinaAct = make_parented<QAction>(tr("Add to Auto DJ Queue as Cortina"), this);
        connect(m_pAutoDJCortinaAct, &QAction::triggered, this, &WTrackMenu::slotAddToAutoDJCortina);
    }

    // The in-place cortina toggle belongs to the Auto DJ queue list, whose model
    // lacks the AddToAutoDJ capability and so is NOT a featureIsEnabled(Feature::
    // AutoDJ) context. Create it unconditionally, keep it hidden by default, and
    // reveal it only for the Auto DJ list in Tango mode (see updateMenus()). Its
    // label flips between "Set as Cortina" and "Set as Track".
    m_pCortinaToggleAct = make_parented<QAction>(tr("Set as Cortina"), this);
    m_pCortinaToggleAct->setVisible(false);
    connect(m_pCortinaToggleAct, &QAction::triggered, this, &WTrackMenu::slotToggleCortina);

    // Reset the Tango set state (mark all unplayed + restart from the top). Like
    // the cortina toggle it lives on the Auto DJ queue list, so create it
    // unconditionally, hidden, and reveal it only there (and only while Auto DJ is
    // stopped and not LIVE) in updateMenus().
    m_pResetAutoDJQueueStateAct = make_parented<QAction>(
            tr("Eject decks and reset AutoDJ queue state"), this);
    m_pResetAutoDJQueueStateAct->setVisible(false);
    connect(m_pResetAutoDJQueueStateAct,
            &QAction::triggered,
            this,
            &WTrackMenu::slotResetAutoDJQueueState);

    // Non-clickable info line with the total duration of the selected tracks.
    // Like the cortina toggle it must also work in the Auto DJ queue list (not a
    // Feature::AutoDJ context), so create it unconditionally and hide it until
    // updateMenus() reveals it in Tango mode.
    m_pSelectionDurationAct = make_parented<QAction>(QString(), this);
    m_pSelectionDurationAct->setEnabled(false);
    m_pSelectionDurationAct->setVisible(false);

    if (featureIsEnabled(Feature::LoadTo)) {
        m_pAddToPreviewDeck = make_parented<QAction>(tr("Preview Deck"), m_pLoadToMenu);
        // currently there is only one preview deck so just map it here.
        QString previewDeckGroup = PlayerManager::groupForPreviewDeck(0);
        connect(m_pAddToPreviewDeck,
                &QAction::triggered,
                this,
                [this, previewDeckGroup] {
                    loadSelectionToGroup(previewDeckGroup);
                });
    }

    if (featureIsEnabled(Feature::Remove)) {
        // Keyboard shortcuts are set here just to have them displayed in the menu.
        // Actual keypress is handled in WTrackTableView::keyPressEvent().
        m_pRemoveAct = make_parented<QAction>(tr("Remove"), this);
        m_pRemoveAct->setShortcut(hideRemoveKeySequence);
        connect(m_pRemoveAct, &QAction::triggered, this, &WTrackMenu::slotRemove);

        m_pRemovePlaylistAct = make_parented<QAction>(tr("Remove from Playlist"), this);
        m_pRemovePlaylistAct->setShortcut(hideRemoveKeySequence);
        connect(m_pRemovePlaylistAct, &QAction::triggered, this, &WTrackMenu::slotRemove);

        m_pRemoveCrateAct = make_parented<QAction>(tr("Remove from Crate"), this);
        m_pRemoveCrateAct->setShortcut(hideRemoveKeySequence);
        connect(m_pRemoveCrateAct, &QAction::triggered, this, &WTrackMenu::slotRemove);
    }

    if (featureIsEnabled(Feature::HideUnhidePurge)) {
        m_pHideAct = make_parented<QAction>(tr("Hide from Library"), this);
        // This is just for having the shortcut displayed next to the action in the menu.
        // The actual keypress is handled in WTrackTableView::keyPressEvent().
        // Note: don't show the hotkey for more than one action
        if (!featureIsEnabled(Feature::Remove)) {
            m_pHideAct->setShortcut(hideRemoveKeySequence);
        }
        connect(m_pHideAct, &QAction::triggered, this, &WTrackMenu::slotHide);

        m_pUnhideAct = make_parented<QAction>(tr("Unhide from Library"), this);
        connect(m_pUnhideAct, &QAction::triggered, this, &WTrackMenu::slotUnhide);

        m_pPurgeAct = make_parented<QAction>(tr("Purge from Library"), this);
        connect(m_pPurgeAct, &QAction::triggered, this, &WTrackMenu::slotPurge);
    }

    if (featureIsEnabled(Feature::RemoveFromDisk)) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
        m_pRemoveFromDiskAct = make_parented<QAction>(tr("Move Track File(s) to Trash"), this);
#else
        m_pRemoveFromDiskAct = make_parented<QAction>(
                tr("Delete Files from Disk"), m_pRemoveFromDiskMenu);
#endif
        connect(m_pRemoveFromDiskAct,
                &QAction::triggered,
                this,
                &WTrackMenu::slotRemoveFromDisk);
    }

    if (featureIsEnabled(Feature::Metadata)) {
        m_pStarRatingAction = make_parented<WStarRatingAction>(this);
        m_pStarRatingAction->setObjectName("RatingAction");
        connect(m_pStarRatingAction,
                &WStarRatingAction::ratingSet,
                this,
                &WTrackMenu::slotSetRating);
    }

    if (featureIsEnabled(Feature::Properties)) {
        m_pPropertiesAct = make_parented<QAction>(tr("Properties"), this);
        // This is just for having the shortcut displayed next to the action
        // when the menu is invoked from the tracks table.
        // The keypress is caught in WTrackTableView::keyPressEvent
        if (m_pTrackModel) {
            m_pPropertiesAct->setShortcut(
                    // TODO(XXX): Qt6 replace enum | with QKeyCombination
                    QKeySequence(
                            static_cast<int>(kPropertiesShortcutModifier) |
                            kPropertiesShortcutKey));
        }
        connect(m_pPropertiesAct, &QAction::triggered, this, &WTrackMenu::slotShowDlgTrackInfo);
    }

    if (featureIsEnabled(Feature::FileBrowser)) {
        m_pFileBrowserAct = make_parented<QAction>(tr("Open in File Browser"), this);
        connect(m_pFileBrowserAct, &QAction::triggered, this, &WTrackMenu::slotOpenInFileBrowser);
    }

    if (featureIsEnabled(Feature::SelectInLibrary)) {
        m_pSelectInLibraryAct = make_parented<QAction>(tr("Select in Library"), this);
        connect(m_pSelectInLibraryAct, &QAction::triggered, this, &WTrackMenu::slotSelectInLibrary);
    }

    if (featureIsEnabled(Feature::Metadata)) {
        m_pImportMetadataFromFileAct =
                make_parented<QAction>(tr("Import From File Tags"), m_pMetadataMenu);
        connect(m_pImportMetadataFromFileAct,
                &QAction::triggered,
                this,
                &WTrackMenu::slotImportMetadataFromFileTags);

        m_pImportMetadataFromMusicBrainzAct =
                make_parented<QAction>(tr("Import From MusicBrainz"), m_pMetadataMenu);
        connect(m_pImportMetadataFromMusicBrainzAct,
                &QAction::triggered,
                this,
                &WTrackMenu::slotShowDlgTagFetcher);

        m_pExportMetadataAct =
                make_parented<QAction>(tr("Export To File Tags"), m_pMetadataMenu);
        connect(m_pExportMetadataAct,
                &QAction::triggered,
                this,
                &WTrackMenu::slotExportMetadataIntoFileTags);

        m_updateInExternalTrackCollections.reserve(
                m_pLibrary->trackCollectionManager()->externalCollections().size());
        for (auto* const pExternalTrackCollection :
                m_pLibrary->trackCollectionManager()->externalCollections()) {
            UpdateExternalTrackCollection updateInExternalTrackCollection;
            updateInExternalTrackCollection.pExternalTrackCollection = pExternalTrackCollection;
            updateInExternalTrackCollection.pAction = make_parented<QAction>(
                    pExternalTrackCollection->name(), m_pMetadataMenu);
            updateInExternalTrackCollection.pAction->setToolTip(
                    pExternalTrackCollection->description());
            m_updateInExternalTrackCollections += updateInExternalTrackCollection;
            connect(updateInExternalTrackCollection.pAction,
                    &QAction::triggered,
                    this,
                    [this, pExternalTrackCollection] {
                        slotUpdateExternalTrackCollection(pExternalTrackCollection);
                    });
        }
    }

    if (featureIsEnabled(Feature::Reset)) {
        // Clear metadata actions
        m_pClearBeatsAction = make_parented<QAction>(tr("BPM and Beatgrid"), m_pClearMetadataMenu);
        connect(m_pClearBeatsAction, &QAction::triggered, this, &WTrackMenu::slotClearBeats);

        m_pClearPlayCountAction = make_parented<QAction>(tr("Play Count"), m_pClearMetadataMenu);
        connect(m_pClearPlayCountAction, &QAction::triggered, this, &WTrackMenu::slotClearPlayCount);

        m_pClearRatingAction = make_parented<QAction>(tr("Rating"), m_pClearMetadataMenu);
        connect(m_pClearRatingAction, &QAction::triggered, this, &WTrackMenu::slotClearRating);

        m_pClearMainCueAction = make_parented<QAction>(tr("Cue Point"), m_pClearMetadataMenu);
        connect(m_pClearMainCueAction, &QAction::triggered, this, &WTrackMenu::slotResetMainCue);

        m_pClearHotCuesAction = make_parented<QAction>(tr("Hotcues"), m_pClearMetadataMenu);
        connect(m_pClearHotCuesAction, &QAction::triggered, this, &WTrackMenu::slotClearHotCues);

        m_pClearIntroCueAction = make_parented<QAction>(tr("Intro"), m_pClearMetadataMenu);
        connect(m_pClearIntroCueAction, &QAction::triggered, this, &WTrackMenu::slotResetIntroCue);

        m_pClearOutroCueAction = make_parented<QAction>(tr("Outro"), m_pClearMetadataMenu);
        connect(m_pClearOutroCueAction, &QAction::triggered, this, &WTrackMenu::slotResetOutroCue);

        m_pClearLoopsAction = make_parented<QAction>(tr("Loops"), m_pClearMetadataMenu);
        connect(m_pClearLoopsAction, &QAction::triggered, this, &WTrackMenu::slotClearLoops);

        m_pClearKeyAction = make_parented<QAction>(tr("Key"), m_pClearMetadataMenu);
        connect(m_pClearKeyAction, &QAction::triggered, this, &WTrackMenu::slotClearKey);

        m_pClearReplayGainAction = make_parented<QAction>(tr("ReplayGain"), m_pClearMetadataMenu);
        connect(m_pClearReplayGainAction, &QAction::triggered, this, &WTrackMenu::slotClearReplayGain);

        m_pClearWaveformAction = make_parented<QAction>(tr("Waveform"), m_pClearMetadataMenu);
        connect(m_pClearWaveformAction, &QAction::triggered, this, &WTrackMenu::slotClearWaveform);

        m_pClearCommentAction = make_parented<QAction>(tr("Comment"), m_pClearMetadataMenu);
        connect(m_pClearCommentAction, &QAction::triggered, this, &WTrackMenu::slotClearComment);

        m_pClearAllMetadataAction = make_parented<QAction>(tr("All"), m_pClearMetadataMenu);
        connect(m_pClearAllMetadataAction, &QAction::triggered, this, &WTrackMenu::slotClearAllMetadata);
    }

    if (featureIsEnabled(Feature::BPM)) {
        m_pBpmLockAction = make_parented<QAction>(tr("Lock BPM"), m_pBPMMenu);
        m_pBpmUnlockAction = make_parented<QAction>(tr("Unlock BPM"), m_pBPMMenu);
        connect(m_pBpmLockAction, &QAction::triggered, this, &WTrackMenu::slotLockBpm);
        connect(m_pBpmUnlockAction, &QAction::triggered, this, &WTrackMenu::slotUnlockBpm);

        //BPM edit actions
        m_pBpmDoubleAction = make_parented<QAction>(tr("Double BPM"), m_pBPMMenu);
        storeActionTextAndScaleInProperties(m_pBpmDoubleAction, 2.0);
        m_pBpmHalveAction = make_parented<QAction>(tr("Halve BPM"), m_pBPMMenu);
        storeActionTextAndScaleInProperties(m_pBpmHalveAction, 0.5);
        m_pBpmTwoThirdsAction = make_parented<QAction>(tr("2/3 BPM"), m_pBPMMenu);
        storeActionTextAndScaleInProperties(m_pBpmTwoThirdsAction, 2.0 / 3.0);
        m_pBpmThreeFourthsAction = make_parented<QAction>(tr("3/4 BPM"), m_pBPMMenu);
        storeActionTextAndScaleInProperties(m_pBpmThreeFourthsAction, 3.0 / 4.0);
        m_pBpmFourThirdsAction = make_parented<QAction>(tr("4/3 BPM"), m_pBPMMenu);
        storeActionTextAndScaleInProperties(m_pBpmFourThirdsAction, 4.0 / 3.0);
        m_pBpmThreeHalvesAction = make_parented<QAction>(tr("3/2 BPM"), m_pBPMMenu);
        storeActionTextAndScaleInProperties(m_pBpmThreeHalvesAction, 3.0 / 2.0);

        connect(m_pBpmDoubleAction, &QAction::triggered, this, [this] {
            slotScaleBpm(mixxx::Beats::BpmScale::Double);
        });
        connect(m_pBpmHalveAction, &QAction::triggered, this, [this] {
            slotScaleBpm(mixxx::Beats::BpmScale::Halve);
        });
        connect(m_pBpmTwoThirdsAction, &QAction::triggered, this, [this] {
            slotScaleBpm(mixxx::Beats::BpmScale::TwoThirds);
        });
        connect(m_pBpmThreeFourthsAction, &QAction::triggered, this, [this] {
            slotScaleBpm(mixxx::Beats::BpmScale::ThreeFourths);
        });
        connect(m_pBpmFourThirdsAction, &QAction::triggered, this, [this] {
            slotScaleBpm(mixxx::Beats::BpmScale::FourThirds);
        });
        connect(m_pBpmThreeHalvesAction, &QAction::triggered, this, [this] {
            slotScaleBpm(mixxx::Beats::BpmScale::ThreeHalves);
        });

        m_pBpmResetAction = make_parented<QAction>(tr("Clear BPM and Beatgrid"), m_pBPMMenu);
        connect(m_pBpmResetAction,
                &QAction::triggered,
                this,
                &WTrackMenu::slotClearBeats);

        m_pBpmUndoAction = make_parented<QAction>(tr("Undo last BPM/beats change"), m_pBPMMenu);
        connect(m_pBpmUndoAction,
                &QAction::triggered,
                this,
                &WTrackMenu::slotUndoBeatsChange);
    }

    if (featureIsEnabled(Feature::Analyze)) {
        m_pAnalyzeAction = make_parented<QAction>(tr("Analyze"), this);
        connect(m_pAnalyzeAction, &QAction::triggered, this, &WTrackMenu::slotAnalyze);

        m_pReanalyzeAction = make_parented<QAction>(tr("Reanalyze"), this);
        connect(m_pReanalyzeAction, &QAction::triggered, this, &WTrackMenu::slotReanalyze);

        m_pReanalyzeConstBpmAction = make_parented<QAction>(tr("Reanalyze (constant BPM)"), this);
        connect(m_pReanalyzeConstBpmAction,
                &QAction::triggered,
                this,
                &WTrackMenu::slotReanalyzeWithFixedTempo);

        m_pReanalyzeVarBpmAction = make_parented<QAction>(tr("Reanalyze (variable BPM)"), this);
        connect(m_pReanalyzeVarBpmAction,
                &QAction::triggered,
                this,
                &WTrackMenu::slotReanalyzeWithVariableTempo);
    }

    // This action is only usable when m_deckGroup is set. That is true only
    // for WTrackmenu instantiated by WTrackProperty and other deck widgets, thus
    // don't create it if a track model is set.
    if (!m_pTrackModel && featureIsEnabled(Feature::UpdateReplayGainFromPregain)) {
        m_pUpdateReplayGainAct = make_parented<QAction>(
                tr("Update ReplayGain from Deck Gain"), m_pClearMetadataMenu);
        connect(m_pUpdateReplayGainAct,
                &QAction::triggered,
                this,
                &WTrackMenu::slotUpdateReplayGainFromPregain);
    }

    if (featureIsEnabled(Feature::Color)) {
        ColorPaletteSettings colorPaletteSettings(m_pConfig);
        m_pColorPickerAction = make_parented<WColorPickerAction>(WColorPicker::Option::AllowNoColor,
                colorPaletteSettings.getTrackColorPalette(),
                m_pColorMenu);
        m_pColorPickerAction->setObjectName("TrackColorPickerAction");
        connect(m_pColorPickerAction,
                &WColorPickerAction::colorPicked,
                this,
                &WTrackMenu::slotColorPicked);
    }
}

void WTrackMenu::setupActions() {
    if (featureIsEnabled(Feature::SearchRelated)) {
        addMenu(m_pSearchRelatedMenu);
    }

    if (featureIsEnabled(Feature::SelectInLibrary)) {
        addAction(m_pSelectInLibraryAct);
    }

    if (featureIsEnabled(Feature::SearchRelated) ||
            featureIsEnabled(Feature::SelectInLibrary)) {
        addSeparator();
    }

    // Selected-tracks duration info line, above the Auto DJ / cortina actions.
    // Shown only in Tango mode (updateMenus); its separator is toggled with it.
    addAction(m_pSelectionDurationAct);
    m_pSelectionDurationSeparator = addSeparator();
    m_pSelectionDurationSeparator->setVisible(false);

    if (featureIsEnabled(Feature::AutoDJ)) {
        addAction(m_pAutoDJBottomAct);
        addAction(m_pAutoDJTopAct);
        addAction(m_pAutoDJReplaceAct);
        addAction(m_pAutoDJCortinaAct);
        addSeparator();
    }

    // Shown only in Tango mode (updateMenus), including in the Auto DJ queue list.
    addAction(m_pCortinaToggleAct);

    // Eject decks and reset AutoDJ queue state: Auto DJ queue list only, revealed
    // in updateMenus() with its own leading separator so it reads as a distinct,
    // deliberate action.
    m_pResetAutoDJQueueStateSeparator = addSeparator();
    m_pResetAutoDJQueueStateSeparator->setVisible(false);
    addAction(m_pResetAutoDJQueueStateAct);

    if (featureIsEnabled(Feature::LoadTo)) {
        m_pLoadToMenu->addMenu(m_pDeckMenu);

        m_pLoadToMenu->addMenu(m_pSamplerMenu);

        if (m_pNumPreviewDecks.get() > 0.0) {
            m_pLoadToMenu->addAction(m_pAddToPreviewDeck);
        }

        addMenu(m_pLoadToMenu);
        addSeparator();
    }

    if (featureIsEnabled(Feature::Playlist)) {
        addMenu(m_pPlaylistMenu);
    }

    if (featureIsEnabled(Feature::Crate)) {
        addMenu(m_pCrateMenu);
    }

    if (featureIsEnabled(Feature::Remove)) {
        if (m_pTrackModel->hasCapabilities(TrackModel::Capability::Remove)) {
            addAction(m_pRemoveAct);
        }
        if (m_pTrackModel->hasCapabilities(TrackModel::Capability::RemovePlaylist)) {
            addAction(m_pRemovePlaylistAct);
        }
        if (m_pTrackModel->hasCapabilities(TrackModel::Capability::RemoveCrate)) {
            addAction(m_pRemoveCrateAct);
        }
    }

    addSeparator();

    if (featureIsEnabled(Feature::BPM)) {
        m_pBPMMenu->addAction(m_pBpmHalveAction);
        m_pBPMMenu->addAction(m_pBpmTwoThirdsAction);
        m_pBPMMenu->addAction(m_pBpmThreeFourthsAction);
        m_pBPMMenu->addAction(m_pBpmFourThirdsAction);
        m_pBPMMenu->addAction(m_pBpmThreeHalvesAction);
        m_pBPMMenu->addAction(m_pBpmDoubleAction);
        m_pBPMMenu->addSeparator();
        m_pBPMMenu->addAction(m_pBpmLockAction);
        m_pBPMMenu->addAction(m_pBpmUnlockAction);
        m_pBPMMenu->addSeparator();
        m_pBPMMenu->addAction(m_pBpmResetAction);
        m_pBPMMenu->addAction(m_pBpmUndoAction);

        addMenu(m_pBPMMenu);
    }

    if (featureIsEnabled(Feature::Metadata)) {
        addAction(m_pStarRatingAction);
    }

    if (featureIsEnabled(Feature::Color)) {
        m_pColorMenu->addAction(m_pColorPickerAction);
        addMenu(m_pColorMenu);
    }

    if (featureIsEnabled(Feature::Metadata)) {
        m_pMetadataMenu->addAction(m_pImportMetadataFromFileAct);
        m_pMetadataMenu->addAction(m_pImportMetadataFromMusicBrainzAct);
        m_pMetadataMenu->addAction(m_pExportMetadataAct);

        for (const auto& updateInExternalTrackCollection :
                std::as_const(m_updateInExternalTrackCollections)) {
            m_pMetadataUpdateExternalCollectionsMenu->addAction(
                    updateInExternalTrackCollection.pAction);
        }
        if (!m_pMetadataUpdateExternalCollectionsMenu->isEmpty()) {
            m_pMetadataMenu->addMenu(m_pMetadataUpdateExternalCollectionsMenu);
            // Enable/disable entries depending on the connection status
            // that may change at runtime.
            connect(m_pMetadataUpdateExternalCollectionsMenu,
                    &QMenu::aboutToShow,
                    this,
                    [this] {
                        for (const auto& updateInExternalTrackCollection :
                                std::as_const(m_updateInExternalTrackCollections)) {
                            updateInExternalTrackCollection.pAction->setEnabled(
                                    updateInExternalTrackCollection
                                            .pExternalTrackCollection
                                            ->isConnected());
                        }
                    });
        }

        m_pMetadataMenu->addMenu(m_pCoverMenu);
        if (featureIsEnabled(Feature::FindOnWeb)) {
            m_pMetadataMenu->addMenu(m_pFindOnWebMenu);
        }
        addSeparator();
        addMenu(m_pMetadataMenu);
    }

    if (featureIsEnabled(Feature::Reset)) {
        m_pClearMetadataMenu->addAction(m_pClearBeatsAction);
        m_pClearMetadataMenu->addAction(m_pClearPlayCountAction);
        m_pClearMetadataMenu->addAction(m_pClearRatingAction);
        m_pClearMetadataMenu->addAction(m_pClearCommentAction);
        m_pClearMetadataMenu->addAction(m_pClearMainCueAction);
        m_pClearMetadataMenu->addAction(m_pClearHotCuesAction);
        m_pClearMetadataMenu->addAction(m_pClearIntroCueAction);
        m_pClearMetadataMenu->addAction(m_pClearOutroCueAction);
        m_pClearMetadataMenu->addAction(m_pClearLoopsAction);
        m_pClearMetadataMenu->addAction(m_pClearKeyAction);
        m_pClearMetadataMenu->addAction(m_pClearReplayGainAction);
        m_pClearMetadataMenu->addAction(m_pClearWaveformAction);
        m_pClearMetadataMenu->addSeparator();
        m_pClearMetadataMenu->addAction(m_pClearAllMetadataAction);
        addMenu(m_pClearMetadataMenu);
    }

    if (featureIsEnabled(Feature::Analyze)) {
        m_pAnalyzeMenu->addAction(m_pAnalyzeAction);
        m_pAnalyzeMenu->addAction(m_pReanalyzeAction);
        m_pAnalyzeMenu->addAction(m_pReanalyzeConstBpmAction);
        m_pAnalyzeMenu->addAction(m_pReanalyzeVarBpmAction);
        addMenu(m_pAnalyzeMenu);
    }

    // This action is created only for menus instantiated by deck widgets (e.g.
    // WTrackProperty) and if UpdateReplayGainFromPregain is supported.
    if (m_pUpdateReplayGainAct) {
        addAction(m_pUpdateReplayGainAct);
    }

    addSeparator();

    if (featureIsEnabled(Feature::HideUnhidePurge)) {
        if (m_pTrackModel->hasCapabilities(TrackModel::Capability::Hide)) {
            addAction(m_pHideAct);
        }
        if (m_pTrackModel->hasCapabilities(TrackModel::Capability::Unhide)) {
            addAction(m_pUnhideAct);
        }
        if (m_pTrackModel->hasCapabilities(TrackModel::Capability::Purge)) {
            addAction(m_pPurgeAct);
        }
    }

    if (featureIsEnabled(Feature::RemoveFromDisk)) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
        addAction(m_pRemoveFromDiskAct);
#else
        m_pRemoveFromDiskMenu->addAction(m_pRemoveFromDiskAct);
        addMenu(m_pRemoveFromDiskMenu);
#endif
    }

    if (featureIsEnabled(Feature::FileBrowser)) {
        addAction(m_pFileBrowserAct);
    }

    if (featureIsEnabled(Feature::Properties)) {
        addSeparator();
        addAction(m_pPropertiesAct);
    }
}

std::pair<bool, bool> WTrackMenu::getTrackBpmLockStates() const {
    bool anyBpmLocked = false;
    bool anyBpmNotLocked = false;
    if (m_pTrackModel) {
        const int column = m_pTrackModel->fieldIndex(LIBRARYTABLE_BPM_LOCK);
        for (const auto& trackIndex : m_trackIndexList) {
            QModelIndex bpmLockedIndex = trackIndex.sibling(trackIndex.row(), column);
            if (bpmLockedIndex.data().toBool()) {
                anyBpmLocked = true;
            } else {
                anyBpmNotLocked = true;
            }
            if (anyBpmLocked && anyBpmNotLocked) {
                break;
            }
        }
    } else if (m_pTrack) {
        anyBpmLocked = m_pTrack->isBpmLocked();
        anyBpmNotLocked = !anyBpmLocked;
    }
    return std::pair<bool, bool>(anyBpmLocked, anyBpmNotLocked);
}

int WTrackMenu::getCommonTrackRating() const {
    VERIFY_OR_DEBUG_ASSERT(!isEmpty()) {
        return 0;
    }
    int commonRating = 0;
    if (m_pTrackModel) {
        const int column =
                m_pTrackModel->fieldIndex(LIBRARYTABLE_RATING);
        commonRating = m_trackIndexList.first()
                               .sibling(m_trackIndexList.first().row(), column)
                               .data()
                               .value<StarRating>()
                               .starCount();
        for (const auto& trackIndex : m_trackIndexList) {
            const auto otherRating =
                    trackIndex.sibling(trackIndex.row(), column)
                            .data()
                            .value<StarRating>()
                            .starCount();
            if (commonRating != otherRating) {
                // Multiple, different ratings
                return 0;
            }
        }
    } else {
        if (!m_pTrack) {
            return 0;
        }
        commonRating = m_pTrack->getRating();
    }
    return commonRating;
}

std::optional<std::optional<mixxx::RgbColor>> WTrackMenu::getCommonTrackColor() const {
    VERIFY_OR_DEBUG_ASSERT(!isEmpty()) {
        return std::nullopt;
    }
    std::optional<mixxx::RgbColor> commonColor;
    if (m_pTrackModel) {
        const int column =
                m_pTrackModel->fieldIndex(LIBRARYTABLE_COLOR);
        commonColor = mixxx::RgbColor::fromQVariant(
                m_trackIndexList.first().sibling(m_trackIndexList.first().row(), column).data());
        for (const auto& trackIndex : m_trackIndexList) {
            const auto otherColor = mixxx::RgbColor::fromQVariant(
                    trackIndex.sibling(trackIndex.row(), column).data());
            if (commonColor != otherColor) {
                // Multiple, different colors
                return std::nullopt;
            }
        }
    } else {
        if (!m_pTrack) {
            return std::nullopt;
        }
        commonColor = m_pTrack->getColor();
    }
    return make_optional(commonColor);
}

CoverInfo WTrackMenu::getCoverInfoOfLastTrack() const {
    VERIFY_OR_DEBUG_ASSERT(!isEmpty()) {
        return CoverInfo();
    }
    if (m_pTrackModel) {
        const QModelIndex lastIndex = m_trackIndexList.last();
        CoverInfo coverInfo;
        coverInfo.source = static_cast<CoverInfo::Source>(
                lastIndex
                        .sibling(
                                lastIndex.row(),
                                m_pTrackModel->fieldIndex(LIBRARYTABLE_COVERART_SOURCE))
                        .data()
                        .toInt());
        coverInfo.type = static_cast<CoverInfo::Type>(
                lastIndex
                        .sibling(
                                lastIndex.row(),
                                m_pTrackModel->fieldIndex(LIBRARYTABLE_COVERART_TYPE))
                        .data()
                        .toInt());
        coverInfo.color = mixxx::RgbColor::fromQVariant(
                lastIndex
                        .sibling(
                                lastIndex.row(),
                                m_pTrackModel->fieldIndex(LIBRARYTABLE_COVERART_COLOR))
                        .data());
        const auto imageDigest =
                lastIndex
                        .sibling(
                                lastIndex.row(),
                                m_pTrackModel->fieldIndex(LIBRARYTABLE_COVERART_DIGEST))
                        .data()
                        .toByteArray();
        const auto legacyHash =
                lastIndex
                        .sibling(
                                lastIndex.row(),
                                m_pTrackModel->fieldIndex(LIBRARYTABLE_COVERART_HASH))
                        .data()
                        .toUInt();
        coverInfo.setImageDigest(imageDigest, legacyHash);
        coverInfo.coverLocation =
                lastIndex
                        .sibling(
                                lastIndex.row(),
                                m_pTrackModel->fieldIndex(LIBRARYTABLE_COVERART_LOCATION))
                        .data()
                        .toString();
        coverInfo.trackLocation =
                lastIndex
                        .sibling(
                                lastIndex.row(),
                                m_pTrackModel->fieldIndex(TRACKLOCATIONSTABLE_LOCATION))
                        .data()
                        .toString();
        return coverInfo;
    } else if (m_pTrack) {
        return m_pTrack->getCoverInfoWithLocation();
    } else {
        // No track, no track model
        return CoverInfo();
    }
}

void WTrackMenu::updateMenus() {
    if (isEmpty()) {
        return;
    }

    // Gray out some stuff if multiple songs were selected.
    const bool singleTrackSelected = getTrackCount() == 1;

    if (featureIsEnabled(Feature::SearchRelated)) {
        // Enable only if we have one valid track pointer.
        // this prevents the cursor getting stuck on this menu in case it gets
        // disabled when encountering a track nullptr in lambda function
        // connected to aboutToShow() signal (see createMenus()).
        // Note: track nullptr can happen when TrackDAO returns nullptr because
        // the selected track references a file referenced by another cached track.
        DEBUG_ASSERT(m_pSearchRelatedMenu);
        const auto pTrack = getFirstTrackPointer();
        m_pSearchRelatedMenu->setEnabled(pTrack != nullptr);
        // TODO Only enable for single track?
    }

    if (featureIsEnabled(Feature::AutoDJ)) {
        // In Tango DJ mode the Auto DJ queue is a pre-arranged, cursor-based set.
        // Adding to the top or replacing it would disrupt the planned milonga, so
        // grey those out and leave only "Add to Auto DJ Queue (bottom)".
        const bool tangoMode = ControlObject::get(ConfigKey(
                                       QStringLiteral("[AutoDJ]"),
                                       QStringLiteral("keep_queue"))) > 0.0;
        m_pAutoDJTopAct->setEnabled(!tangoMode);
        m_pAutoDJReplaceAct->setEnabled(!tangoMode);
    }

    // The in-place cortina toggle is independent of Feature::AutoDJ so it can
    // appear in the Auto DJ queue list (whose model has no AddToAutoDJ capability).
    // Scope it to that list and to Tango mode. The label flips so the action reads
    // as "Set as Cortina" for normal tracks and "Set as Track" for cortinas.
    {
        const bool tangoMode = ControlObject::get(ConfigKey(
                                       QStringLiteral("[AutoDJ]"),
                                       QStringLiteral("keep_queue"))) > 0.0;
        const bool show = tangoMode && isCortinaList();
        m_pCortinaToggleAct->setVisible(show);
        if (show) {
            const TrackIdList trackIds = getTrackIds();
            bool allCortina = !trackIds.isEmpty();
            for (const auto& trackId : trackIds) {
                if (!CortinaRegistry::instance().contains(trackId)) {
                    allCortina = false;
                    break;
                }
            }
            m_pCortinaToggleAct->setText(
                    allCortina ? tr("Set as Track") : tr("Set as Cortina"));
        }
    }

    // Eject decks and reset AutoDJ queue state: Auto DJ queue list, Tango mode, and
    // only while Auto DJ is stopped and not in LIVE mode - so it can never wipe a
    // running set or eject a deck out from under one.
    {
        const bool tangoMode = ControlObject::get(ConfigKey(
                                       QStringLiteral("[AutoDJ]"),
                                       QStringLiteral("keep_queue"))) > 0.0;
        const bool autoDJRunning = ControlObject::get(ConfigKey(
                                           QStringLiteral("[AutoDJ]"),
                                           QStringLiteral("enabled"))) > 0.0;
        const bool liveMode = ControlObject::get(ConfigKey(
                                      QStringLiteral("[AutoDJ]"),
                                      QStringLiteral("live_mode"))) > 0.0;
        const bool show = tangoMode && isCortinaList() &&
                !autoDJRunning && !liveMode;
        m_pResetAutoDJQueueStateAct->setVisible(show);
        if (m_pResetAutoDJQueueStateSeparator) {
            m_pResetAutoDJQueueStateSeparator->setVisible(show);
        }
    }

    // Selected-tracks duration info line. Tango mode only, shown wherever the Auto
    // DJ add actions are available (library/crate/playlist) and also in the Auto DJ
    // queue list (Feature::AutoDJ is false there, hence the isCortinaList() check).
    {
        const bool tangoMode = ControlObject::get(ConfigKey(
                                       QStringLiteral("[AutoDJ]"),
                                       QStringLiteral("keep_queue"))) > 0.0;
        const bool show = tangoMode &&
                (featureIsEnabled(Feature::AutoDJ) || isCortinaList());
        m_pSelectionDurationAct->setVisible(show);
        if (m_pSelectionDurationSeparator) {
            m_pSelectionDurationSeparator->setVisible(show);
        }
        if (show) {
            const QModelIndexList& indices = getTrackIndices();
            const mixxx::Duration total = sumTracksDuration(m_pTrackModel, indices);
            m_pSelectionDurationAct->setText(
                    tr("%1  (%2)")
                            .arg(mixxx::DurationBase::formatTime(
                                         total.toDoubleSeconds()),
                                    tr("%n track(s)",
                                            "",
                                            static_cast<int>(indices.size()))));
        }
    }

    if (featureIsEnabled(Feature::LoadTo)) {
        // Enable menus only for single track
        int iNumDecks = static_cast<int>(m_pNumDecks.get());
        m_pDeckMenu->clear();
        m_pDeckMenu->setEnabled(singleTrackSelected);
        if (singleTrackSelected && iNumDecks > 0) {
            for (int i = 1; i <= iNumDecks; ++i) {
                // PlayerManager::groupForDeck is 0-indexed.
                QString deckGroup = PlayerManager::groupForDeck(i - 1);
                bool deckPlaying = ControlObject::get(
                                           ConfigKey(deckGroup, "play")) > 0.0;
                bool allowLoadTrackIntoPlayingDeck = false;
                if (m_pConfig->exists(kConfigKeyLoadWhenDeckPlaying)) {
                    int loadWhenDeckPlaying =
                            m_pConfig->getValueString(kConfigKeyLoadWhenDeckPlaying).toInt();
                    switch (static_cast<LoadWhenDeckPlaying>(loadWhenDeckPlaying)) {
                    case LoadWhenDeckPlaying::Allow:
                    case LoadWhenDeckPlaying::AllowButStopDeck:
                        allowLoadTrackIntoPlayingDeck = true;
                        break;
                    case LoadWhenDeckPlaying::Reject:
                        break;
                    }
                } else {
                    // support older version of this flag
                    allowLoadTrackIntoPlayingDeck = m_pConfig->getValue<bool>(
                            ConfigKey("[Controls]", "AllowTrackLoadToPlayingDeck"));
                }
                bool deckEnabled =
                        (!deckPlaying || allowLoadTrackIntoPlayingDeck) &&
                        singleTrackSelected;
                auto pAction = make_parented<QAction>(tr("Deck %1").arg(i), this);
                pAction->setEnabled(deckEnabled);
                m_pDeckMenu->addAction(pAction);
                connect(pAction, &QAction::triggered, this, [this, deckGroup] {
                    loadSelectionToGroup(deckGroup);
                });
            }
        }

        int iNumSamplers = static_cast<int>(m_pNumSamplers.get());
        const int maxSamplersPerMenu = 16;
        m_pSamplerMenu->clear();
        m_pSamplerMenu->setEnabled(singleTrackSelected);
        if (singleTrackSelected && iNumSamplers > 0) {
            QMenu* pMenu = m_pSamplerMenu;
            int samplersInMenu = 0;
            for (int i = 1; i <= iNumSamplers; ++i) {
                if (samplersInMenu == maxSamplersPerMenu) {
                    samplersInMenu = 0;
                    int limit = iNumSamplers > i + 15 ? i + 15 : iNumSamplers;
                    const QString label = samplerTrString(i) + QStringLiteral("- %1").arg(limit);
                    pMenu = make_parented<QMenu>(label, m_pSamplerMenu);
                    m_pSamplerMenu->addMenu(pMenu);
                }
                samplersInMenu++;
                // PlayerManager::groupForSampler is 0-indexed.
                QString samplerGroup = PlayerManager::groupForSampler(i - 1);
                bool samplerPlaying = ControlObject::get(
                                              ConfigKey(samplerGroup, "play")) > 0.0;
                bool samplerEnabled = !samplerPlaying && singleTrackSelected;
                auto pAction = make_parented<QAction>(samplerTrString(i), pMenu);
                pAction->setEnabled(samplerEnabled);
                pMenu->addAction(pAction);
                connect(pAction,
                        &QAction::triggered,
                        this,
                        [this, samplerGroup] {
                            loadSelectionToGroup(samplerGroup);
                        });
            }
        }
    }

    if (featureIsEnabled(Feature::Playlist)) {
        // Playlist menu is lazy loaded on hover by slotPopulatePlaylistMenu
        // to avoid unnecessary database queries
        m_bPlaylistMenuLoaded = false;
    }

    if (featureIsEnabled(Feature::Crate)) {
        // Crate menu is lazy loaded on hover by slotPopulateCrateMenu
        // to avoid unnecessary database queries
        m_bCrateMenuLoaded = false;
    }

    if (featureIsEnabled(Feature::Remove)) {
        bool locked = m_pTrackModel->hasCapabilities(TrackModel::Capability::Locked);
        if (m_pTrackModel->hasCapabilities(TrackModel::Capability::Remove)) {
            m_pRemoveAct->setEnabled(!locked);
        }
        if (m_pTrackModel->hasCapabilities(TrackModel::Capability::RemovePlaylist)) {
            m_pRemovePlaylistAct->setEnabled(!locked);
        }
        if (m_pTrackModel->hasCapabilities(TrackModel::Capability::RemoveCrate)) {
            m_pRemoveCrateAct->setEnabled(!locked);
        }
    }

    if (featureIsEnabled(Feature::Metadata)) {
        m_pImportMetadataFromMusicBrainzAct->setEnabled(singleTrackSelected);

        // We use the last selected track for the cover art context to be
        // consistent with selectionChanged above.
        m_pCoverMenu->setCoverArt(getCoverInfoOfLastTrack());
        m_pMetadataMenu->addMenu(m_pCoverMenu);
    }

    if (featureIsEnabled(Feature::Analyze)) {
        bool useFixedTempo = m_pConfig->getValue<bool>(
                ConfigKey("[BPM]", "BeatDetectionFixedTempoAssumption"));
        // Since we already have a 'Reanalyze' action that uses the configured
        // default, we hide the redundant menu as per suggestion:
        // https://github.com/mixxxdj/mixxx/pull/10931#issuecomment-1262559750
        m_pReanalyzeConstBpmAction->setVisible(!useFixedTempo);
        m_pReanalyzeVarBpmAction->setVisible(useFixedTempo);
    }

    if (featureIsEnabled(Feature::Reset) ||
            featureIsEnabled(Feature::BPM)) {
        bool anyBpmLocked;
        bool anyBpmNotLocked;
        std::tie(anyBpmLocked, anyBpmNotLocked) = getTrackBpmLockStates();
        if (featureIsEnabled(Feature::Reset)) {
            m_pClearBeatsAction->setEnabled(!anyBpmLocked);
        }
        if (featureIsEnabled(Feature::BPM)) {
            m_pBpmUnlockAction->setEnabled(anyBpmLocked);
            m_pBpmLockAction->setEnabled(anyBpmNotLocked);
            m_pBpmDoubleAction->setEnabled(!anyBpmLocked);
            m_pBpmHalveAction->setEnabled(!anyBpmLocked);
            m_pBpmTwoThirdsAction->setEnabled(!anyBpmLocked);
            m_pBpmThreeFourthsAction->setEnabled(!anyBpmLocked);
            m_pBpmFourThirdsAction->setEnabled(!anyBpmLocked);
            m_pBpmThreeHalvesAction->setEnabled(!anyBpmLocked);
            m_pBpmResetAction->setEnabled(!anyBpmLocked);
            m_pBpmUndoAction->setEnabled(!anyBpmLocked && canUndoBeatsChange());

            // Append scaled BPM preview for single selection
            if (singleTrackSelected) {
                TrackPointer pTrack;
                if (m_pTrackModel) {
                    pTrack = getFirstTrackPointer();
                } else if (m_pTrack) {
                    pTrack = m_pTrack;
                }
                if (pTrack) {
                    const double bpm = pTrack->getBpm();
                    appendBpmPreviewtoBpmAction(m_pBpmDoubleAction, bpm);
                    appendBpmPreviewtoBpmAction(m_pBpmHalveAction, bpm);
                    appendBpmPreviewtoBpmAction(m_pBpmTwoThirdsAction, bpm);
                    appendBpmPreviewtoBpmAction(m_pBpmThreeFourthsAction, bpm);
                    appendBpmPreviewtoBpmAction(m_pBpmFourThirdsAction, bpm);
                    appendBpmPreviewtoBpmAction(m_pBpmThreeHalvesAction, bpm);
                }
            }
        }
    }

    // This action is created only for menus instantiated by deck widgets (e.g.
    // WTrackProperty) and if UpdateReplayGainFromPregain is supported.
    // Disable it if no deck group was set.
    if (m_pUpdateReplayGainAct) {
        m_pUpdateReplayGainAct->setEnabled(!m_deckGroup.isEmpty());
    }

    if (featureIsEnabled(Feature::Color)) {
        m_pColorPickerAction->setColorPalette(
                ColorPaletteSettings(m_pConfig).getTrackColorPalette());

        // Resize Menu to fit changed palette
        QResizeEvent resizeEvent(QSize(), m_pColorMenu->size());
        qApp->sendEvent(m_pColorMenu, &resizeEvent);

        const auto commonColor = getCommonTrackColor();
        if (commonColor) {
            m_pColorPickerAction->setSelectedColor(*commonColor);
        } else {
            m_pColorPickerAction->resetSelectedColor();
        }
    }

    if (featureIsEnabled(Feature::HideUnhidePurge)) {
        bool locked = m_pTrackModel->hasCapabilities(TrackModel::Capability::Locked);
        // Note: Hide action is enabled regardless the locked state.
        // Like in Tracks, in locked playlists A confirmation dialog pops up:
        // "Hiding track ... will remove it from the following playlists: ..."
        if (m_pTrackModel->hasCapabilities(TrackModel::Capability::Unhide)) {
            m_pUnhideAct->setEnabled(!locked);
        }
        if (m_pTrackModel->hasCapabilities(TrackModel::Capability::Purge)) {
            m_pPurgeAct->setEnabled(!locked);
        }
    }

    if (featureIsEnabled(Feature::RemoveFromDisk)) {
        if (m_pTrackModel) {
            bool locked = m_pTrackModel->hasCapabilities(TrackModel::Capability::Locked);
            if (m_pTrackModel->hasCapabilities(TrackModel::Capability::RemoveFromDisk)) {
                m_pRemoveFromDiskAct->setEnabled(!locked);
            }
        }
    }

    if (featureIsEnabled(Feature::SelectInLibrary)) {
        bool enabled = false;
        if (m_pTrack) {
            enabled = m_pLibrary->isTrackIdInCurrentLibraryView(m_pTrack->getId());
        }
        m_pSelectInLibraryAct->setEnabled(enabled);
    }

    if (featureIsEnabled(Feature::Metadata)) {
        // Might be needed to resize Menu to fit the star rating
        // QResizeEvent resizeEvent(QSize(), m_pStarRatingAction->sizeHint());
        // qApp->sendEvent(m_pStarRatingAction, &resizeEvent);
        m_pStarRatingAction->setRating(getCommonTrackRating());
    }

    if (featureIsEnabled(Feature::Properties)) {
        m_pPropertiesAct->setEnabled(true);
    }

    if (featureIsEnabled(Feature::FindOnWeb)) {
        const auto pTrack = getFirstTrackPointer();
        const bool enableMenu = pTrack ? WFindOnWebMenu::hasEntriesForTrack(*pTrack) : false;
        m_pFindOnWebMenu->setEnabled(enableMenu);
    }
}

void WTrackMenu::loadTrack(
        const TrackPointer& pTrack, const QString& deckGroup) {
    // This asserts that this function is only accessible when a track model is not set,
    // thus maintaining only the TrackPointerList in state and avoiding storing
    // duplicate state with TrackIdList and QModelIndexList.
    VERIFY_OR_DEBUG_ASSERT(!m_pTrackModel) {
        return;
    }

    // Clean all forms of track store
    clearTrackSelection();

    if (!pTrack) {
        return;
    }
    m_pTrack = pTrack;
    m_deckGroup = deckGroup;
    updateMenus();
}

void WTrackMenu::loadTrackModelIndices(
        const QModelIndexList& trackIndexList) {
    // This asserts that this function is only accessible when a track model is set,
    // thus maintaining only the QModelIndexList in state and avoiding storing
    // duplicate state with TrackIdList and TrackPointerList.
    VERIFY_OR_DEBUG_ASSERT(m_pTrackModel) {
        return;
    }

    // Clean all forms of track store
    clearTrackSelection();

    m_trackIndexList = trackIndexList;
    updateMenus();
}

TrackIdList WTrackMenu::getTrackIds() const {
    TrackIdList trackIds;
    if (m_pTrackModel) {
        trackIds.reserve(m_trackIndexList.size());
        for (const auto& index : m_trackIndexList) {
            const auto trackId = m_pTrackModel->getTrackId(index);
            if (!trackId.isValid()) {
                // Skip unavailable tracks
                continue;
            }
            trackIds.push_back(trackId);
        }
    } else {
        if (m_pTrack) {
            const auto trackId = m_pTrack->getId();
            DEBUG_ASSERT(trackId.isValid());
            trackIds.push_back(trackId);
        }
    }
    return trackIds;
}

QList<TrackRef> WTrackMenu::getTrackRefs() const {
    QList<TrackRef> trackRefs;
    if (m_pTrackModel) {
        trackRefs.reserve(m_trackIndexList.size());
        for (const auto& index : m_trackIndexList) {
            auto trackRef = TrackRef::fromFilePath(
                    m_pTrackModel->getTrackLocation(index),
                    m_pTrackModel->getTrackId(index));
            if (!trackRef.isValid()) {
                // Skip unavailable tracks
                continue;
            }
            trackRefs.push_back(std::move(trackRef));
        }
    } else if (m_pTrack) {
        auto trackRef = TrackRef::fromFileInfo(
                m_pTrack->getFileInfo(),
                m_pTrack->getId());
        trackRefs.push_back(std::move(trackRef));
    }
    return trackRefs;
}

TrackPointer WTrackMenu::getFirstTrackPointer() const {
    if (m_pTrackModel) {
        for (const auto& index : m_trackIndexList) {
            const auto pTrack = m_pTrackModel->getTrack(index);
            if (pTrack) {
                return pTrack;
            }
            // Skip unavailable tracks
        }
        return TrackPointer();
    }
    return m_pTrack;
}

TrackPointerList WTrackMenu::getTrackPointers() const {
    TrackPointerList tracks;
    if (m_pTrackModel) {
        for (const auto& index : m_trackIndexList) {
            const auto pTrack = m_pTrackModel->getTrack(index);
            if (pTrack) {
                tracks.append(pTrack);
            }
        }
    } else {
        tracks.append(m_pTrack);
    }
    return tracks;
}

std::unique_ptr<mixxx::TrackPointerIterator> WTrackMenu::newTrackPointerIterator() const {
    if (m_pTrackModel) {
        if (m_trackIndexList.isEmpty()) {
            return nullptr;
        }
        // m_pTrackModel must not be modified during the iteration,
        // neither directly nor indirectly through signals!!!
        return std::make_unique<mixxx::TrackPointerModelIterator>(
                m_pTrackModel,
                m_trackIndexList);
    } else if (m_pTrack) {
        return std::make_unique<mixxx::TrackPointerListIterator>(
                TrackPointerList{m_pTrack});
    }
    return nullptr;
}

int WTrackMenu::applyTrackPointerOperation(
        const QString& progressLabelText,
        const mixxx::TrackPointerOperation* pTrackPointerOperation,
        mixxx::ModalTrackBatchOperationProcessor::Mode operationMode) const {
    const auto pTrackPointerIter = newTrackPointerIterator();
    if (!pTrackPointerIter) {
        // Empty, i.e. nothing to do
        return 0;
    }
    mixxx::ModalTrackBatchOperationProcessor modalOperation(
            pTrackPointerOperation,
            operationMode);
    return modalOperation.processTracks(
            progressLabelText,
            m_pLibrary->trackCollectionManager(),
            pTrackPointerIter.get());
}

const QModelIndexList& WTrackMenu::getTrackIndices() const {
    // Indices are associated with a TrackModel. Can only be obtained
    // if a TrackModel is available.
    DEBUG_ASSERT(m_pTrackModel);
    return m_trackIndexList;
}

void WTrackMenu::slotOpenInFileBrowser() {
    const auto trackRefs = getTrackRefs();
    // Warn when opening many files to prevent system hangs
    constexpr int kMaxFilesToOpenInBrowser = 10;
    if (getTrackCount() > kMaxFilesToOpenInBrowser) {
        QMessageBox::StandardButton reply = QMessageBox::question(
                nullptr,
                tr("Open Many Files in File Browser"),
                tr("You are about to open %n files in the file browser. "
                   "This may slow down or hang your system. "
                   "Are you sure you want to continue?",
                        "",
                        getTrackCount()),
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No);

        if (reply != QMessageBox::Yes) {
            return;
        }
    }

    QStringList locations;
    locations.reserve(trackRefs.size());
    for (const auto& trackRef : trackRefs) {
        locations << trackRef.getLocation();
    }
    mixxx::DesktopHelper::openInFileBrowser(locations);
}

void WTrackMenu::slotSelectInLibrary() {
    if (m_pTrack) {
        emit m_pLibrary->selectTrack(m_pTrack->getId());
    }
}

namespace {

class ImportMetadataFromFileTagsTrackPointerOperation : public mixxx::TrackPointerOperation {
  public:
    explicit ImportMetadataFromFileTagsTrackPointerOperation(
            const UserSettings& userSettings)
            : m_params(SyncTrackMetadataParams::readFromUserSettings(userSettings)) {
    }

  private:
    void doApply(
            const TrackPointer& pTrack) const override {
        // The user has explicitly requested to reload metadata from the file
        // to override the information within Mixxx! Custom cover art must be
        // reloaded separately.
        SoundSourceProxy(pTrack).updateTrackFromSource(
                SoundSourceProxy::UpdateTrackFromSourceMode::Always,
                m_params);
    }

    const SyncTrackMetadataParams m_params;
};

} // anonymous namespace

void WTrackMenu::slotUpdateReplayGainFromPregain() {
    VERIFY_OR_DEBUG_ASSERT(m_pTrack) {
        return;
    }
    VERIFY_OR_DEBUG_ASSERT(!m_deckGroup.isEmpty()) {
        return;
    }

    const double gain = ControlObject::get(ConfigKey(m_deckGroup, "pregain"));
    // Gain is at unity already, ignore and return.
    if (gain == 1.0) {
        return;
    }
    m_pTrack->adjustReplayGainFromPregain(gain, m_deckGroup);
}

void WTrackMenu::slotImportMetadataFromFileTags() {
    const auto progressLabelText =
            tr("Importing metadata of %n track(s) from file tags", "", getTrackCount());
    const auto trackOperator =
            ImportMetadataFromFileTagsTrackPointerOperation(*m_pConfig);
    applyTrackPointerOperation(
            progressLabelText,
            &trackOperator,
            // Update the database to reflect the recent changes. This is
            // crucial for additional metadata like custom tags that are
            // directly fetched from the database for certain use cases!
            mixxx::ModalTrackBatchOperationProcessor::Mode::ApplyAndSave);
}

namespace {

class ExportMetadataIntoFileTagsTrackPointerOperation : public mixxx::TrackPointerOperation {
  private:
    void doApply(
            const TrackPointer& pTrack) const override {
        pTrack->markForMetadataExport();
    }
};

} // anonymous namespace

void WTrackMenu::slotExportMetadataIntoFileTags() {
    // Export of metadata is deferred until all references to the
    // corresponding track object have been dropped. Otherwise
    // writing to files that are still used for playback might
    // cause crashes or at least audible glitches!
    mixxx::DlgTrackMetadataExport::showMessageBoxOncePerSession();

    const auto progressLabelText =
            tr("Marking metadata of %n track(s) to be exported into file tags",
                    "",
                    getTrackCount());
    const auto trackOperator =
            ExportMetadataIntoFileTagsTrackPointerOperation();
    applyTrackPointerOperation(
            progressLabelText,
            &trackOperator);
}

void WTrackMenu::slotUpdateExternalTrackCollection(
        ExternalTrackCollection* externalTrackCollection) {
    VERIFY_OR_DEBUG_ASSERT(externalTrackCollection) {
        return;
    }

    externalTrackCollection->updateTracks(getTrackRefs());
}

void WTrackMenu::slotPopulatePlaylistMenu() {
    // The user may open the Playlist submenu, move their cursor away, then
    // return to the Playlist submenu before exiting the track context menu.
    // Avoid querying the database multiple times in that case.
    if (m_bPlaylistMenuLoaded) {
        return;
    }
    m_pPlaylistMenu->clear();
    const PlaylistDAO& playlistDao = m_pLibrary->trackCollectionManager()
                                             ->internalCollection()
                                             ->getPlaylistDAO();
    const QList<QPair<int, QString>> playlists =
            playlistDao.getPlaylists(PlaylistDAO::PLHT_NOT_HIDDEN);

    for (const auto& [id, name] : playlists) {
        // No leak because making the menu the parent means they will be
        // auto-deleted
        int plId = id;
        auto pAction = make_parented<QAction>(
                mixxx::escapeTextPropertyWithoutShortcuts(name),
                m_pPlaylistMenu);
        bool locked = playlistDao.isPlaylistLocked(plId);
        pAction->setEnabled(!locked);
        m_pPlaylistMenu->addAction(pAction);
        connect(pAction,
                &QAction::triggered,
                this,
                [this, plId] {
                    addSelectionToPlaylist(plId);
                });
    }
    m_pPlaylistMenu->addSeparator();
    auto newPlaylistAction = make_parented<QAction>(tr("Create New Playlist"), m_pPlaylistMenu);
    m_pPlaylistMenu->addAction(newPlaylistAction);
    connect(newPlaylistAction, &QAction::triggered, this, [this] { addSelectionToPlaylist(-1); });
    m_bPlaylistMenuLoaded = true;
}

void WTrackMenu::addSelectionToPlaylist(int iPlaylistId) {
    const TrackIdList trackIds = getTrackIds();
    if (trackIds.isEmpty()) {
        qWarning() << "No tracks selected for playlist";
        return;
    }

    PlaylistDAO& playlistDao = m_pLibrary->trackCollectionManager()
                                       ->internalCollection()
                                       ->getPlaylistDAO();

    if (iPlaylistId == -1) { // i.e. a new playlist is suppose to be created
        QString name;
        bool validNameGiven = false;

        do {
            bool ok = false;
            name = QInputDialog::getText(nullptr,
                    tr("Create New Playlist"),
                    tr("Enter name for new playlist:"),
                    QLineEdit::Normal,
                    tr("New Playlist"),
                    &ok)
                           .trimmed();
            if (!ok) {
                return;
            }
            if (playlistDao.getPlaylistIdFromName(name) != -1) {
                QMessageBox::warning(nullptr,
                        tr("Playlist Creation Failed"),
                        tr("A playlist by that name already exists."));
            } else if (name.isEmpty()) {
                QMessageBox::warning(nullptr,
                        tr("Playlist Creation Failed"),
                        tr("A playlist cannot have a blank name."));
            } else {
                validNameGiven = true;
            }
        } while (!validNameGiven);
        iPlaylistId = playlistDao.createPlaylist(name); //-1 is changed to the new playlist ID return from the DAO
        if (iPlaylistId == -1) {
            QMessageBox::warning(nullptr,
                    tr("Playlist Creation Failed"),
                    tr("An unknown error occurred while creating playlist: ") + name);
            return;
        }
    }

    // TODO(XXX): Care whether the append succeeded.
    m_pLibrary->trackCollectionManager()->unhideTracks(trackIds);
    playlistDao.appendTracksToPlaylist(trackIds, iPlaylistId);
}

void WTrackMenu::slotPopulateCrateMenu() {
    // The user may open the Crate submenu, move their cursor away, then
    // return to the Crate submenu before exiting the track context menu.
    // Avoid querying the database multiple times in that case.
    if (m_bCrateMenuLoaded) {
        return;
    }
    m_pCrateMenu->clear();
    const TrackIdList trackIds = getTrackIds();

    CrateSummarySelectResult allCrates(
            m_pLibrary->trackCollectionManager()
                    ->internalCollection()
                    ->crates()
                    .selectCratesWithTrackCount(trackIds));

    CrateSummary crate;
    while (allCrates.populateNext(&crate)) {
        auto pAction = make_parented<QWidgetAction>(
                m_pCrateMenu);
        // Use a custom QCheckBox with fixed hover behavior.
        auto pCheckBox = make_parented<WMenuCheckBox>(
                mixxx::escapeTextPropertyWithoutShortcuts(crate.getName()),
                m_pCrateMenu);
        pCheckBox->setProperty("crateId", QVariant::fromValue(crate.getId()));
        pCheckBox->setEnabled(!crate.isLocked());
        // Strangely, the normal styling of QActions does not automatically
        // apply to QWidgetActions. The :selected pseudo-state unfortunately
        // does not work with QWidgetAction. :hover works for selecting items
        // with the mouse, but not with the keyboard. :focus works for the
        // keyboard but with the mouse, the last clicked item keeps the style
        // after the mouse cursor is moved to hover over another item.

        // ronso0 Disabling this stylesheet allows to override the OS style
        // of the :hover and :focus state.
        //        pCheckBox->setStyleSheet(
        //            QString("QCheckBox {color: %1;}").arg(
        //                    pCheckBox->palette().text().color().name()) + "\n" +
        //            QString("QCheckBox:hover {background-color: %1;}").arg(
        //                    pCheckBox->palette().highlight().color().name()));
        pAction->setEnabled(!crate.isLocked());
        pAction->setDefaultWidget(pCheckBox.get());

        if (crate.getTrackCount() == 0) {
            pCheckBox->setChecked(false);
        } else if (crate.getTrackCount() == (uint)trackIds.length()) {
            pCheckBox->setChecked(true);
        } else {
            pCheckBox->setTristate(true);
            pCheckBox->setCheckState(Qt::PartiallyChecked);
        }

        m_pCrateMenu->addAction(pAction.get());
        connect(pAction.get(), &QAction::triggered, this, [this, pCheckBox{pCheckBox.get()}] { updateSelectionCrates(pCheckBox); });

#if QT_VERSION >= QT_VERSION_CHECK(6, 7, 0)
        connect(pCheckBox.get(),
                &QCheckBox::checkStateChanged,
                this,
                [this, pCheckBox{pCheckBox.get()}] {
                    updateSelectionCrates(pCheckBox);
                });
#else
        connect(pCheckBox.get(), &QCheckBox::stateChanged, this, [this, pCheckBox{pCheckBox.get()}] { updateSelectionCrates(pCheckBox); });
#endif
    }
    m_pCrateMenu->addSeparator();
    auto newCrateAction = make_parented<QAction>(tr("Add to New Crate"), m_pCrateMenu);
    m_pCrateMenu->addAction(newCrateAction);
    connect(newCrateAction, &QAction::triggered, this, &WTrackMenu::addSelectionToNewCrate);
    m_bCrateMenuLoaded = true;
}

void WTrackMenu::updateSelectionCrates(QWidget* pWidget) {
    auto* pCheckBox = qobject_cast<QCheckBox*>(pWidget);
    VERIFY_OR_DEBUG_ASSERT(pCheckBox) {
        qWarning() << "crateId is not of CrateId type";
        return;
    }
    CrateId crateId = pCheckBox->property("crateId").value<CrateId>();

    const TrackIdList trackIds = getTrackIds();

    if (trackIds.isEmpty()) {
        qWarning() << "No tracks selected for crate";
        return;
    }

    // we need to disable tristate again as the mixed state will now be gone and can't be brought back
    pCheckBox->setTristate(false);
    if (!pCheckBox->isChecked()) {
        if (crateId.isValid()) {
            m_pLibrary->trackCollectionManager()
                    ->internalCollection()
                    ->removeCrateTracks(crateId, trackIds);
        }
    } else {
        if (!crateId.isValid()) { // i.e. a new crate is suppose to be created
            crateId = CrateFeatureHelper(
                    m_pLibrary->trackCollectionManager()->internalCollection(), m_pConfig)
                              .createEmptyCrate();
        }
        if (crateId.isValid()) {
            m_pLibrary->trackCollectionManager()->unhideTracks(trackIds);
            m_pLibrary->trackCollectionManager()
                    ->internalCollection()
                    ->addCrateTracks(crateId, trackIds);
        }
    }
}

void WTrackMenu::addSelectionToNewCrate() {
    const TrackIdList trackIds = getTrackIds();

    if (trackIds.isEmpty()) {
        qWarning() << "No tracks selected for crate";
        return;
    }

    CrateId crateId = CrateFeatureHelper(
            m_pLibrary->trackCollectionManager()->internalCollection(), m_pConfig)
                              .createEmptyCrate();

    if (crateId.isValid()) {
        m_pLibrary->trackCollectionManager()->unhideTracks(trackIds);
        m_pLibrary->trackCollectionManager()
                ->internalCollection()
                ->addCrateTracks(crateId, trackIds);
    }
}

void WTrackMenu::addToAnalysis(AnalyzerTrack::Options options) {
    const TrackIdList trackIds = getTrackIds();
    if (trackIds.empty()) {
        qWarning() << "No tracks selected for analysis";
        return;
    }

    QList<AnalyzerScheduledTrack> tracks;
    for (auto trackId : trackIds) {
        AnalyzerScheduledTrack track(trackId, options);
        tracks.append(track);
    }

    emit m_pLibrary->analyzeTracks(tracks);
}

void WTrackMenu::slotAnalyze() {
    addToAnalysis();
}

void WTrackMenu::slotReanalyze() {
    clearBeats();
    addToAnalysis();
}

void WTrackMenu::slotReanalyzeWithFixedTempo() {
    clearBeats();
    AnalyzerTrack::Options options;
    options.useFixedTempo = true;
    addToAnalysis(options);
}

void WTrackMenu::slotReanalyzeWithVariableTempo() {
    clearBeats();
    AnalyzerTrack::Options options;
    options.useFixedTempo = false;
    addToAnalysis(options);
}

void WTrackMenu::slotLockBpm() {
    lockBpm(true);
}

void WTrackMenu::slotUnlockBpm() {
    lockBpm(false);
}

namespace {

class ScaleBpmTrackPointerOperation : public mixxx::TrackPointerOperation {
  public:
    explicit ScaleBpmTrackPointerOperation(mixxx::Beats::BpmScale bpmScale)
            : m_bpmScale(bpmScale) {
    }

  private:
    void doApply(
            const TrackPointer& pTrack) const override {
        if (pTrack->isBpmLocked()) {
            return;
        }
        const mixxx::BeatsPointer pBeats = pTrack->getBeats();
        if (!pBeats) {
            return;
        }
        const auto scaledBeats = pBeats->tryScale(m_bpmScale);
        if (!scaledBeats) {
            return;
        }
        pTrack->trySetBeats(*scaledBeats);
    }

    const mixxx::Beats::BpmScale m_bpmScale;
};

} // anonymous namespace

void WTrackMenu::slotScaleBpm(mixxx::Beats::BpmScale scale) {
    const auto progressLabelText =
            tr("Scaling BPM of %n track(s)", "", getTrackCount());
    const auto trackOperator =
            ScaleBpmTrackPointerOperation(scale);
    applyTrackPointerOperation(
            progressLabelText,
            &trackOperator);
}

namespace {

class UndoBeatsChangeTrackPointerOperation : public mixxx::TrackPointerOperation {
  public:
    explicit UndoBeatsChangeTrackPointerOperation() {
    }

  private:
    void doApply(
            const TrackPointer& pTrack) const override {
        if (pTrack->isBpmLocked()) {
            return;
        }
        pTrack->undoBeatsChange();
    }
};

} // anonymous namespace

void WTrackMenu::slotUndoBeatsChange() {
    const auto progressLabelText =
            tr("Undo BPM/beats change of %n track(s)", "", getTrackCount());
    const auto trackOperator =
            UndoBeatsChangeTrackPointerOperation();
    applyTrackPointerOperation(
            progressLabelText,
            &trackOperator);
}

bool WTrackMenu::canUndoBeatsChange() const {
    const auto pTrackPointerIterator = newTrackPointerIterator();
    if (!pTrackPointerIterator) {
        // Empty, i.e. nothing to do
        return false;
    }
    while (auto nextTrackPointer = pTrackPointerIterator->nextItem()) {
        const auto pTrack = *nextTrackPointer;
        if (!pTrack->canUndoBeatsChange()) {
            return false;
        }
    }
    return true;
}

namespace {

class LockBpmTrackPointerOperation : public mixxx::TrackPointerOperation {
  public:
    explicit LockBpmTrackPointerOperation(bool lock)
            : m_lock(lock) {
    }

  private:
    void doApply(
            const TrackPointer& pTrack) const override {
        pTrack->setBpmLocked(m_lock);
    }

    const bool m_lock;
};

} // anonymous namespace

void WTrackMenu::lockBpm(bool lock) {
    const auto progressLabelText = lock
            ? tr("Locking BPM of %n track(s)", "", getTrackCount())
            : tr("Unlocking BPM of %n track(s)", "", getTrackCount());
    const auto trackOperator =
            LockBpmTrackPointerOperation(lock);
    applyTrackPointerOperation(
            progressLabelText,
            &trackOperator);
}

namespace {

class SetRatingTrackPointerOperation : public mixxx::TrackPointerOperation {
  public:
    explicit SetRatingTrackPointerOperation(const int rating)
            : m_rating(rating) {
    }

  private:
    void doApply(
            const TrackPointer& pTrack) const override {
        pTrack->setRating(m_rating);
    }

    const int m_rating;
};

} // anonymous namespace

void WTrackMenu::slotSetRating(int rating) {
    if (!mixxx::TrackRecord::isValidRating(rating)) {
        return;
    }

    const auto progressLabelText =
            tr("Setting rating of %n track(s)", "", getTrackCount());
    const auto trackOperator =
            SetRatingTrackPointerOperation(rating);
    applyTrackPointerOperation(
            progressLabelText,
            &trackOperator);

    hide();
}

namespace {

class SetColorTrackPointerOperation : public mixxx::TrackPointerOperation {
  public:
    explicit SetColorTrackPointerOperation(const mixxx::RgbColor::optional_t& color)
            : m_color(color) {
    }

  private:
    void doApply(
            const TrackPointer& pTrack) const override {
        pTrack->setColor(m_color);
    }

    const mixxx::RgbColor::optional_t m_color;
};

} // anonymous namespace

void WTrackMenu::slotColorPicked(const mixxx::RgbColor::optional_t& color) {
    const auto progressLabelText =
            tr("Setting color of %n track(s)", "", getTrackCount());
    const auto trackOperator =
            SetColorTrackPointerOperation(color);
    applyTrackPointerOperation(
            progressLabelText,
            &trackOperator);

    hide();
}

void WTrackMenu::loadSelectionToGroup(const QString& group, bool play) {
    TrackPointer pTrack = getFirstTrackPointer();
    if (!pTrack) {
        return;
    }

    // If the track load override is disabled, check to see if a track is
    // playing before trying to load it
    if (!(m_pConfig->getValueString(
                           ConfigKey("[Controls]", "AllowTrackLoadToPlayingDeck"))
                        .toInt())) {
        // TODO(XXX): Check for other than just the first preview deck.
        if (group != "[PreviewDeck1]" &&
                ControlObject::get(ConfigKey(group, "play")) > 0.0) {
            return;
        }
    }

    // TODO: load track from this class without depending on
    // external slot to load track
    emit loadTrackToPlayer(pTrack, group, play);
}

namespace {

class ResetPlayCounterTrackPointerOperation : public mixxx::TrackPointerOperation {
  private:
    void doApply(
            const TrackPointer& pTrack) const override {
        pTrack->resetPlayCounter();
    }
};

} // anonymous namespace

//slot for reset played count, sets count to 0 of one or more tracks
void WTrackMenu::slotClearPlayCount() {
    const auto progressLabelText =
            tr("Resetting play count of %n track(s)", "", getTrackCount());
    const auto trackOperator =
            ResetPlayCounterTrackPointerOperation();
    applyTrackPointerOperation(
            progressLabelText,
            &trackOperator);
}

namespace {

class ResetBeatsTrackPointerOperation : public mixxx::TrackPointerOperation {
  private:
    void doApply(
            const TrackPointer& pTrack) const override {
        pTrack->trySetBeats(mixxx::BeatsPointer());
    }
};

} // anonymous namespace

void WTrackMenu::clearBeats() {
    const auto progressLabelText =
            tr("Resetting beats of %n track(s)", "", getTrackCount());
    const auto trackOperator =
            ResetBeatsTrackPointerOperation();
    applyTrackPointerOperation(
            progressLabelText,
            &trackOperator);
}

void WTrackMenu::slotClearBeats() {
    clearBeats();
}

namespace {

class ResetRatingTrackPointerOperation : public mixxx::TrackPointerOperation {
  private:
    void doApply(
            const TrackPointer& pTrack) const override {
        pTrack->resetRating();
    }
};

} // anonymous namespace

//slot for reset played count, sets count to 0 of one or more tracks
void WTrackMenu::slotClearRating() {
    const auto progressLabelText =
            tr("Clearing rating of %n track(s)", "", getTrackCount());
    const auto trackOperator =
            ResetRatingTrackPointerOperation();
    applyTrackPointerOperation(
            progressLabelText,
            &trackOperator);
}

namespace {

class ClearCommentTrackPointerOperation : public mixxx::TrackPointerOperation {
  private:
    void doApply(
            const TrackPointer& pTrack) const override {
        pTrack->clearComment();
    }
};

} // anonymous namespace

//slot for clearing the comment field of one or more tracks
void WTrackMenu::slotClearComment() {
    const auto progressLabelText =
            tr("Clearing comment of %n track(s)", "", getTrackCount());
    const auto trackOperator =
            ClearCommentTrackPointerOperation();
    applyTrackPointerOperation(
            progressLabelText,
            &trackOperator);
}

namespace {

class RemoveCuesOfTypeTrackPointerOperation : public mixxx::TrackPointerOperation {
  public:
    explicit RemoveCuesOfTypeTrackPointerOperation(mixxx::CueType cueType)
            : m_cueType(cueType) {
    }

  private:
    void doApply(
            const TrackPointer& pTrack) const override {
        pTrack->removeCuesOfType(m_cueType);
    }

    const mixxx::CueType m_cueType;
};

class ResetMainCueTrackPointerOperation : public mixxx::TrackPointerOperation {
  public:
    explicit ResetMainCueTrackPointerOperation(UserSettingsPointer pConfig)
            : m_pConfig(pConfig) {
    }

  private:
    void doApply(
            const TrackPointer& pTrack) const override {
        pTrack->removeCuesOfType(mixxx::CueType::MainCue);
        CuePointer pN60dBSound = pTrack->findCueByType(mixxx::CueType::N60dBSound);
        if (pN60dBSound) {
            mixxx::audio::FramePos firstSound = pN60dBSound->getPosition();
            if (firstSound.isValid()) {
                AnalyzerSilence::setupMainAndIntroCue(pTrack.get(), firstSound, m_pConfig.data());
            }
        }
    }

    UserSettingsPointer m_pConfig;
};

class ResetIntroTrackPointerOperation : public mixxx::TrackPointerOperation {
  public:
    explicit ResetIntroTrackPointerOperation(UserSettingsPointer pConfig)
            : m_pConfig(pConfig) {
    }

  private:
    void doApply(
            const TrackPointer& pTrack) const override {
        pTrack->removeCuesOfType(mixxx::CueType::Intro);
        CuePointer pN60dBSound = pTrack->findCueByType(mixxx::CueType::N60dBSound);
        if (pN60dBSound) {
            mixxx::audio::FramePos firstSound = pN60dBSound->getPosition();
            if (firstSound.isValid()) {
                AnalyzerSilence::setupMainAndIntroCue(pTrack.get(), firstSound, m_pConfig.data());
            }
        }
    }

    UserSettingsPointer m_pConfig;
};

class ResetOutroTrackPointerOperation : public mixxx::TrackPointerOperation {
  public:
    explicit ResetOutroTrackPointerOperation() {
    }

  private:
    void doApply(
            const TrackPointer& pTrack) const override {
        pTrack->removeCuesOfType(mixxx::CueType::Outro);
        CuePointer pN60dBSound = pTrack->findCueByType(mixxx::CueType::N60dBSound);
        if (pN60dBSound) {
            mixxx::audio::FramePos lastSound = pN60dBSound->getEndPosition();
            if (lastSound.isValid()) {
                AnalyzerSilence::setupOutroCue(pTrack.get(), lastSound);
            }
        }
    }
};

} // anonymous namespace

void WTrackMenu::slotResetMainCue() {
    const auto progressLabelText =
            tr("Removing main cue from %n track(s)", "", getTrackCount());
    const auto trackOperator =
            ResetMainCueTrackPointerOperation(m_pConfig);
    applyTrackPointerOperation(
            progressLabelText,
            &trackOperator);
}

void WTrackMenu::slotResetOutroCue() {
    const auto progressLabelText =
            tr("Removing outro cue from %n track(s)", "", getTrackCount());
    const auto trackOperator =
            ResetOutroTrackPointerOperation();
    applyTrackPointerOperation(
            progressLabelText,
            &trackOperator);
}

void WTrackMenu::slotResetIntroCue() {
    const auto progressLabelText =
            tr("Removing intro cue from %n track(s)", "", getTrackCount());
    const auto trackOperator =
            ResetIntroTrackPointerOperation(m_pConfig);
    applyTrackPointerOperation(
            progressLabelText,
            &trackOperator);
}

void WTrackMenu::slotClearLoops() {
    const auto progressLabelText =
            tr("Removing loop cues from %n track(s)", "", getTrackCount());
    const auto trackOperator =
            RemoveCuesOfTypeTrackPointerOperation(mixxx::CueType::Loop);
    applyTrackPointerOperation(
            progressLabelText,
            &trackOperator);
}

void WTrackMenu::slotClearHotCues() {
    const auto progressLabelText =
            tr("Removing hot cues from %n track(s)", "", getTrackCount());
    const auto trackOperator =
            RemoveCuesOfTypeTrackPointerOperation(mixxx::CueType::HotCue);
    applyTrackPointerOperation(
            progressLabelText,
            &trackOperator);
}

namespace {

class ResetKeysTrackPointerOperation : public mixxx::TrackPointerOperation {
  private:
    void doApply(
            const TrackPointer& pTrack) const override {
        pTrack->resetKeys();
    }
};

} // anonymous namespace

void WTrackMenu::slotClearKey() {
    const auto progressLabelText =
            tr("Resetting keys of %n track(s)", "", getTrackCount());
    const auto trackOperator =
            ResetKeysTrackPointerOperation();
    applyTrackPointerOperation(
            progressLabelText,
            &trackOperator);
}

namespace {

class ResetReplayGainTrackPointerOperation : public mixxx::TrackPointerOperation {
  private:
    void doApply(
            const TrackPointer& pTrack) const override {
        pTrack->setReplayGain(mixxx::ReplayGain());
    }
};

} // anonymous namespace

void WTrackMenu::slotClearReplayGain() {
    const auto progressLabelText =
            tr("Resetting replay gain of %n track(s)", "", getTrackCount());
    const auto trackOperator =
            ResetReplayGainTrackPointerOperation();
    applyTrackPointerOperation(
            progressLabelText,
            &trackOperator);
}

namespace {

class ResetWaveformTrackPointerOperation : public mixxx::TrackPointerOperation {
  public:
    explicit ResetWaveformTrackPointerOperation(AnalysisDao& analysisDao)
            : m_analysisDao(analysisDao) {
    }

  private:
    void doApply(
            const TrackPointer& pTrack) const override {
        m_analysisDao.deleteAnalysesForTrack(pTrack->getId());
        pTrack->setWaveform(WaveformPointer());
        pTrack->setWaveformSummary(WaveformPointer());
        // We Remove the invisible AudibleSound cue here as well, because the
        // same reasons that apply for reanalyze of the waveforms applies also
        // for the AudibleSound cue.
        pTrack->removeCuesOfType(mixxx::CueType::N60dBSound);
    }

    AnalysisDao& m_analysisDao;
};

} // anonymous namespace

void WTrackMenu::slotClearWaveform() {
    const auto progressLabelText =
            tr("Resetting waveform of %n track(s)", "", getTrackCount());
    AnalysisDao& analysisDao =
            m_pLibrary->trackCollectionManager()->internalCollection()->getAnalysisDAO();
    const auto trackOperator =
            ResetWaveformTrackPointerOperation(analysisDao);
    applyTrackPointerOperation(
            progressLabelText,
            &trackOperator);
}

namespace {

class ClearAllPerformanceMetadataTrackPointerOperation : public mixxx::TrackPointerOperation {
  public:
    explicit ClearAllPerformanceMetadataTrackPointerOperation(AnalysisDao& analysisDao)
            : m_removeMainCue(mixxx::CueType::MainCue),
              m_removeIntroCue(mixxx::CueType::Intro),
              m_removeOutroCue(mixxx::CueType::Outro),
              m_removeHotCues(mixxx::CueType::HotCue),
              m_removeLoopCues(mixxx::CueType::Loop),
              m_resetWaveform(analysisDao) {
    }

  private:
    void doApply(
            const TrackPointer& pTrack) const override {
        m_resetBeats.apply(pTrack);
        m_resetPlayCounter.apply(pTrack);
        m_removeMainCue.apply(pTrack);
        m_removeHotCues.apply(pTrack);
        m_removeLoopCues.apply(pTrack);
        m_resetKeys.apply(pTrack);
        m_resetReplayGain.apply(pTrack);
        m_resetWaveform.apply(pTrack);
        m_resetRating.apply(pTrack);
        m_removeIntroCue.apply(pTrack);
        m_removeOutroCue.apply(pTrack);
    }

    const ResetBeatsTrackPointerOperation m_resetBeats;
    const ResetPlayCounterTrackPointerOperation m_resetPlayCounter;
    const RemoveCuesOfTypeTrackPointerOperation m_removeMainCue;
    const RemoveCuesOfTypeTrackPointerOperation m_removeIntroCue;
    const RemoveCuesOfTypeTrackPointerOperation m_removeOutroCue;
    const RemoveCuesOfTypeTrackPointerOperation m_removeHotCues;
    const RemoveCuesOfTypeTrackPointerOperation m_removeLoopCues;
    const ResetKeysTrackPointerOperation m_resetKeys;
    const ResetReplayGainTrackPointerOperation m_resetReplayGain;
    const ResetWaveformTrackPointerOperation m_resetWaveform;
    const ResetRatingTrackPointerOperation m_resetRating;
};

} // anonymous namespace

void WTrackMenu::slotClearAllMetadata() {
    const auto progressLabelText =
            tr("Resetting all performance metadata of %n track(s)", "", getTrackCount());
    AnalysisDao& analysisDao =
            m_pLibrary->trackCollectionManager()->internalCollection()->getAnalysisDAO();
    const auto trackOperator =
            ClearAllPerformanceMetadataTrackPointerOperation(analysisDao);
    applyTrackPointerOperation(
            progressLabelText,
            &trackOperator);
}

namespace {

class RemoveTrackFilesFromDiskTrackPointerOperation : public mixxx::TrackPointerOperation {
  public:
    const QList<TrackRef>& getTracksToPurge() const {
        return mTracksToPurge;
    }
    const QList<QString>& getTracksToKeep() const {
        return mTracksToKeep;
    }

  private:
    mutable QList<TrackRef> mTracksToPurge;
    mutable QList<QString> mTracksToKeep;

    void doApply(
            const TrackPointer& pTrack) const override {
        auto trackRef = TrackRef::fromFileInfo(
                pTrack->getFileInfo(),
                pTrack->getId());
        VERIFY_OR_DEBUG_ASSERT(trackRef.isValid()) {
            return;
        }
        QString location = pTrack->getLocation();
        QFile file(location);
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
        if (file.exists() && !file.moveToTrash()) {
#else
        if (file.exists() && !file.remove()) {
#endif
            // Deletion failed, log warning and queue location for the
            // Failed Deletions warning.
            qWarning()
                    << "Queued file"
                    << location
                    << "could not be deleted. Track is not purged";
            mTracksToKeep.append(location);
        } else {
            // File doesn't exist or was deleted.
            // Note: we must NOT purge every single track here since
            // TrackDAO::afterPurgingTracks would enforce a track model update (select())
            // So we add it to the purge queue and purge all tracks at once
            // in slotRemoveFromDisk() afterwards.
            mTracksToPurge.append(trackRef);
        }
    }
};

} // anonymous namespace

void WTrackMenu::slotRemoveFromDisk() {
    QStringList locations;
    if (m_pTrackModel) {
        const auto trackRefs = getTrackRefs();
        locations.reserve(trackRefs.size());
        for (const auto& trackRef : trackRefs) {
            QString location = trackRef.getLocation();
            locations.append(location);
        }
        locations.removeDuplicates();
    } else if (m_pTrack) {
        QString location = m_pTrack->getLocation();
        locations.append(location);
    } else {
        return;
    }

    {
        QDialog dlgDelConfirm;

        // Prepare the delete confirmation dialog.
        // First, create the list view for the files to be deleted
        // NOTE(ronso0) We could also make this a table to allow showing
        // artist and title if file names don't suffice to identify tracks.
        auto pDelListWidget = make_parented<QListWidget>(&dlgDelConfirm);
        pDelListWidget->setSizePolicy(QSizePolicy(QSizePolicy::Minimum,
                QSizePolicy::MinimumExpanding));
        pDelListWidget->setFocusPolicy(Qt::ClickFocus);
        pDelListWidget->addItems(locations);
        mixxx::widgethelper::growListWidget(*pDelListWidget, *this);

        QString delWarningText;
        if (m_pTrackModel) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
            delWarningText = tr("Move these files to the trash bin?");
#else
            delWarningText = tr("Permanently delete these files from disk?") +
                    QStringLiteral("<br><br><b>") +
                    tr("This can not be undone!") + QStringLiteral("</b>");
#endif
        } else { // track menu of track labels
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)

            delWarningText = tr("Move this track file to the trash bin?");
#else
            delWarningText =
                    tr("Permanently delete this track file from disk?") +
                    QStringLiteral("<br><br><b>") +
                    tr("This can not be undone!") + QStringLiteral("</b>");
#endif
        }
        delWarningText.append(QStringLiteral("<br><br>"));
        if (m_pTrackModel) {
            delWarningText.append(tr(
                    "All decks where these tracks are loaded will be "
                    "stopped and the tracks will be ejected."));
        } else {
            delWarningText.append(tr(
                    "All decks where this track is loaded will be "
                    "stopped and the track will be ejected."));
        }

        // Setup the warning message and dialog buttons
        auto pDelWarning = make_parented<QLabel>(&dlgDelConfirm);
        pDelWarning->setText(delWarningText);
        pDelWarning->setTextFormat(Qt::RichText);
        pDelWarning->setSizePolicy(QSizePolicy(QSizePolicy::Minimum,
                QSizePolicy::Minimum));

        auto pDelButtons = make_parented<QDialogButtonBox>(&dlgDelConfirm);
        QPushButton* cancelBtn = pDelButtons->addButton(
                tr("Cancel"),
                QDialogButtonBox::RejectRole);
        QPushButton* deleteBtn = pDelButtons->addButton(
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
                tr("Delete Files"),
#else
                tr("Okay"),
#endif
                QDialogButtonBox::AcceptRole);
        cancelBtn->setDefault(true);

        // Populate the main layout
        auto pDelLayout = make_parented<QVBoxLayout>(&dlgDelConfirm);
        pDelLayout->addWidget(pDelListWidget);
        pDelLayout->addWidget(pDelWarning);
        pDelLayout->addWidget(pDelButtons);

        dlgDelConfirm.setModal(true); // just to be sure
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
        dlgDelConfirm.setWindowTitle(tr("Delete Track Files"));
#else
        dlgDelConfirm.setWindowTitle(tr("Move Track File(s) to Trash?"));
#endif
        // This is required after customizing the buttons, otherwise neither button
        // would close the dialog.
        connect(cancelBtn, &QPushButton::clicked, &dlgDelConfirm, &QDialog::reject);
        connect(deleteBtn, &QPushButton::clicked, &dlgDelConfirm, &QDialog::accept);
        dlgDelConfirm.setLayout(pDelLayout);

        if (dlgDelConfirm.exec() == QDialog::Rejected) {
            return;
        }
    }

    // Try to keep a usable index for navigation if the track is in the
    // current track view.
    bool restoreViewState = false;
    if (m_pTrack && m_pLibrary->isTrackIdInCurrentLibraryView(m_pTrack->getId())) {
        restoreViewState = true;
        emit saveCurrentViewState();
    }
    // Stop all affected decks and eject tracks.
    const TrackPointerList tracks = getTrackPointers();
    const QStringList groups = PlayerInfo::instance().getPlayerGroupsWithTracksLoaded(tracks);
    for (const QString& group : groups) {
        ControlObject::set(ConfigKey(group, "stop"), 1.0);
        ControlObject::set(ConfigKey(group, "eject"), 1.0);
        ControlObject::set(ConfigKey(group, "eject"), 0.0);
    }

    // Set up and initiate the track batch operation
    const auto progressLabelText =
#if QT_VERSION < QT_VERSION_CHECK(5, 15, 0)
            tr("Removing %n track file(s) from disk...",
#else
            tr("Moving %n track file(s) to trash...",
#endif
                    "",
                    getTrackCount());
    const auto trackOperator =
            RemoveTrackFilesFromDiskTrackPointerOperation();
    applyTrackPointerOperation(
            progressLabelText,
            &trackOperator);

    // Purge deleted tracks and show deletion summary message.
    const QList<TrackRef> tracksToPurge(trackOperator.getTracksToPurge());
    if (!tracksToPurge.isEmpty()) {
        // Purge only those tracks whose files have actually been deleted.
        m_pLibrary->trackCollectionManager()->purgeTracks(tracksToPurge);

        // Show purge summary message
        QMessageBox msgBoxPurgeTracks;
        msgBoxPurgeTracks.setIcon(QMessageBox::Information);
        QString msgTitle;
        QString msgText;
        if (m_pTrackModel) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
            msgTitle = tr("Track Files Deleted");
#else
            msgTitle = tr("Track Files Moved To Trash");
#endif
            msgText =
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
                    tr("%1 track files were moved to trash and purged "
                       "from the Mixxx database.")
#else
                    tr("%1 track files were deleted from disk and purged "
                       "from the Mixxx database.")
#endif
                            .arg(QString::number(tracksToPurge.length())) +
                    QStringLiteral("<br><br>") +
                    tr("Note: if you are in the Computer or Recording view you "
                       "need to click the current view again to see changes.");
        } else {
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
            msgTitle = tr("Track File Moved To Trash");
            msgText = tr(
                    "Track file was moved to trash and purged "
                    "from the Mixxx database.");
#else
            msgTitle = tr("Track File Deleted");
            msgText = tr(
                    "Track file was deleted from disk and purged "
                    "from the Mixxx database.");
#endif
        }
        msgBoxPurgeTracks.setWindowTitle(msgTitle);
        msgBoxPurgeTracks.setText(msgText);
        msgBoxPurgeTracks.setTextFormat(Qt::RichText);
        msgBoxPurgeTracks.setStandardButtons(QMessageBox::Ok);
        msgBoxPurgeTracks.exec();
    }

    const QList<QString> tracksToKeep(trackOperator.getTracksToKeep());
    if (tracksToKeep.isEmpty()) {
        if (m_pTrackModel || restoreViewState) {
            // All selected tracks could be processed. Finish!
            emit restoreCurrentViewStateOrIndex();
        }
        return;
    }

    QDialog dlgNotDeleted;

    // Else show a message with a list of tracks that could not be deleted.
    auto pNotDeletedLabel = make_parented<QLabel>(&dlgNotDeleted);
    QString msgText;
    if (m_pTrackModel) {
        msgText =
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
                tr("The following %1 file(s) could not be moved to trash")
#else
                tr("The following %1 file(s) could not be deleted from disk")
#endif
                        .arg(QString::number(
                                tracksToKeep.length()));
    } else {
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
        msgText = tr("This track file could not be moved to trash");
#else
        msgText = tr("This track file could not be deleted from disk");
#endif
    }
    pNotDeletedLabel->setText(msgText);
    pNotDeletedLabel->setTextFormat(Qt::RichText);

    auto pNotDeletedListWidget = make_parented<QListWidget>(&dlgNotDeleted);
    pNotDeletedListWidget->setFocusPolicy(Qt::ClickFocus);
    pNotDeletedListWidget->addItems(tracksToKeep);
    mixxx::widgethelper::growListWidget(*pNotDeletedListWidget, *this);

    auto pNotDeletedButtons = make_parented<QDialogButtonBox>(&dlgNotDeleted);
    QPushButton* closeBtn = pNotDeletedButtons->addButton(
            tr("Close"),
            QDialogButtonBox::AcceptRole);

    auto pNotDeletedLayout = make_parented<QVBoxLayout>(&dlgNotDeleted);
    pNotDeletedLayout->addWidget(pNotDeletedLabel);
    pNotDeletedLayout->addWidget(pNotDeletedListWidget);
    pNotDeletedLayout->addWidget(pNotDeletedButtons);

    dlgNotDeleted.setModal(true);
    dlgNotDeleted.setWindowTitle(tr("Remaining Track File(s)"));
    dlgNotDeleted.setLayout(pNotDeletedLayout);
    // Required for being able to close the dialog
    connect(closeBtn, &QPushButton::clicked, &dlgNotDeleted, &QDialog::close);
    dlgNotDeleted.exec();
    emit restoreCurrentViewStateOrIndex();
}

void WTrackMenu::slotShowDlgTrackInfo() {
    if (isEmpty()) {
        return;
    }

    if (m_pTrackModel && getTrackCount() > 1) {
        // Use the batch editor.
        // Create a fresh dialog on invocation.
        m_pDlgTrackInfoMulti = std::make_unique<DlgTrackInfoMulti>(
                m_pConfig);
        connect(m_pDlgTrackInfoMulti.get(),
                &QDialog::finished,
                this,
                [this]() {
                    if (m_pDlgTrackInfoMulti.get() == sender()) {
                        m_pDlgTrackInfoMulti.release()->deleteLater();
                        // clear the track property name
                        m_trackProperty.clear();
                    }
                });
        QList<TrackPointer> tracks;
        tracks.reserve(getTrackCount());
        for (const auto& index : m_trackIndexList) {
            const auto pTrack = m_pTrackModel->getTrack(index);
            if (pTrack) {
                tracks.append(pTrack);
            }
            // Skip unavailable tracks
        }
        m_pDlgTrackInfoMulti->loadTracks(tracks);
        m_pDlgTrackInfoMulti->show();
        m_pDlgTrackInfoMulti->focusField(m_trackProperty);
    } else {
        // Use the single-track editor with Next/Prev buttons and DlgTagFetcher.
        // Create a fresh dialog on invocation.
        m_pDlgTrackInfo = std::make_unique<DlgTrackInfo>(
                m_pConfig,
                m_pTrackModel);
        connect(m_pDlgTrackInfo.get(),
                &QDialog::finished,
                this,
                [this]() {
                    if (m_pDlgTrackInfo.get() == sender()) {
                        m_pDlgTrackInfo.release()->deleteLater();
                        // clear the track property name
                        m_trackProperty.clear();
                    }
                });
        // Method getFirstTrackPointer() is not applicable here!
        // DlgTrackInfo relies on a track model for certain operations,
        // for example show/hide the Next/Prev buttons.
        // It can be loaded with either an index (must have a model),
        // or a TrackPointer (must NOT have a model then).
        if (m_pTrackModel) {
            m_pDlgTrackInfo->loadTrack(m_trackIndexList.at(0));
        } else {
            m_pDlgTrackInfo->loadTrack(m_pTrack);
        }
        m_pDlgTrackInfo->show();
        m_pDlgTrackInfo->focusField(m_trackProperty);
    }
}

void WTrackMenu::slotShowDlgTagFetcher() {
    if (isEmpty()) {
        return;
    }
    // Create a fresh dialog on invocation
    m_pDlgTagFetcher = std::make_unique<DlgTagFetcher>(
            m_pConfig, m_pTrackModel);
    connect(m_pDlgTagFetcher.get(),
            &QDialog::finished,
            this,
            [this]() {
                if (m_pDlgTagFetcher.get() == sender()) {
                    m_pDlgTagFetcher.release()->deleteLater();
                }
            });
    // Method getFirstTrackPointer() is not applicable here!
    if (m_pTrackModel) {
        m_pDlgTagFetcher->loadTrack(m_trackIndexList.at(0));
    } else {
        m_pDlgTagFetcher->loadTrack(m_pTrack);
    }
    m_pDlgTagFetcher->show();
}

void WTrackMenu::slotAddToAutoDJBottom() {
    // append to auto DJ
    addToAutoDJ(PlaylistDAO::AutoDJSendLoc::BOTTOM);
}

void WTrackMenu::slotAddToAutoDJTop() {
    addToAutoDJ(PlaylistDAO::AutoDJSendLoc::TOP);
}

void WTrackMenu::slotAddToAutoDJCortina() {
    // Add to the bottom of the queue like any other track, then tag the selected
    // tracks as cortinas (session-only) so the Auto DJ list shows the blue
    // "!!!CORTINA!!!" styling.
    addToAutoDJ(PlaylistDAO::AutoDJSendLoc::BOTTOM);
    const TrackIdList trackIds = getTrackIds();
    for (const auto& trackId : trackIds) {
        CortinaRegistry::instance().mark(trackId);
    }
}

void WTrackMenu::slotToggleCortina() {
    // Flip the selected track(s) between cortina and normal track in place. Lets
    // the DJ fix a mistakenly-tagged track (the registry is keyed by track id, so
    // a re-added track keeps its old mark) or promote/demote without re-adding.
    // Mirror the label logic: if every selection is already a cortina, clear them
    // all; otherwise mark them all.
    const TrackIdList trackIds = getTrackIds();
    bool allCortina = !trackIds.isEmpty();
    for (const auto& trackId : trackIds) {
        if (!CortinaRegistry::instance().contains(trackId)) {
            allCortina = false;
            break;
        }
    }
    for (const auto& trackId : trackIds) {
        if (allCortina) {
            CortinaRegistry::instance().unmark(trackId);
        } else {
            CortinaRegistry::instance().mark(trackId);
        }
    }
}

void WTrackMenu::slotResetAutoDJQueueState() {
    // Deliberate, confirmed action: restart the Tango set from the top. Marks
    // every queued track unplayed (reverting the grey "played" colour), resets
    // the play cursor to the first track and clears the decks, so a fully-played
    // set can be replayed from a genuinely clean slate.
    const auto answer = QMessageBox::question(this,
            tr("Eject decks and reset AutoDJ queue state"),
            tr("Eject the tracks loaded on the decks, mark all tracks in the "
               "Auto DJ queue as unplayed and restart the set from the "
               "top?\n\nThis does not change your play counts."),
            QMessageBox::Yes | QMessageBox::Cancel,
            QMessageBox::Cancel);
    if (answer != QMessageBox::Yes) {
        return;
    }
    // Clear the decks first so nothing is left cued from the previous run. This
    // action is only offered while Auto DJ is stopped and outside LIVE mode, so
    // no deck can be ejected out from under a running set. Stop before eject to
    // match how the library handles ejecting a loaded deck.
    const int numDecks = PlayerInfo::instance().numDecks();
    for (int deck = 0; deck < numDecks; ++deck) {
        // PlayerManager::groupForDeck is 0-indexed.
        const QString group = PlayerManager::groupForDeck(deck);
        ControlObject::set(ConfigKey(group, QStringLiteral("stop")), 1.0);
        ControlObject::set(ConfigKey(group, QStringLiteral("eject")), 1.0);
        ControlObject::set(ConfigKey(group, QStringLiteral("eject")), 0.0);
    }
    // The AutoDJProcessor owns the play cursor and the queue model, so route the
    // reset through its control (it clears the played flags and the cursor).
    ControlObject::set(
            ConfigKey(QStringLiteral("[AutoDJ]"),
                    QStringLiteral("reset_queue_state")),
            1.0);
}

void WTrackMenu::slotAddToAutoDJReplace() {
    addToAutoDJ(PlaylistDAO::AutoDJSendLoc::REPLACE);
}

void WTrackMenu::addToAutoDJ(PlaylistDAO::AutoDJSendLoc loc) {
    const TrackIdList trackIds = getTrackIds();
    if (trackIds.empty()) {
        qWarning() << "No tracks selected for AutoDJ";
        return;
    }

    PlaylistDAO& playlistDao = m_pLibrary->trackCollectionManager()
                                       ->internalCollection()
                                       ->getPlaylistDAO();

    // TODO(XXX): Care whether the append succeeded.
    m_pLibrary->trackCollectionManager()->unhideTracks(trackIds);
    playlistDao.addTracksToAutoDJQueue(trackIds, loc);
}

namespace {

class SetCoverInfoTrackPointerOperation : public mixxx::TrackPointerOperation {
  public:
    explicit SetCoverInfoTrackPointerOperation(CoverInfoRelative&& coverInfo)
            : m_coverInfo(std::move(coverInfo)) {
    }

  private:
    void doApply(
            const TrackPointer& pTrack) const override {
        pTrack->setCoverInfo(m_coverInfo);
    }

    const CoverInfoRelative m_coverInfo;
};

} // anonymous namespace

void WTrackMenu::slotCoverInfoSelected(CoverInfoRelative coverInfo) {
    const auto progressLabelText =
            tr("Setting cover art of %n track(s)", "", getTrackCount());
    const auto trackOperator =
            SetCoverInfoTrackPointerOperation(std::move(coverInfo));
    applyTrackPointerOperation(
            progressLabelText,
            &trackOperator);
}

namespace {

class ReloadCoverInfoTrackPointerOperation : public mixxx::TrackPointerOperation {
  private:
    void doApply(
            const TrackPointer& pTrack) const override {
        m_coverInfoGuesser.guessAndSetCoverInfoForTrack(pTrack);
    }

    mutable CoverInfoGuesser m_coverInfoGuesser;
};

} // anonymous namespace

void WTrackMenu::slotReloadCoverArt() {
    const auto progressLabelText =
            tr("Reloading cover art of %n track(s)", "", getTrackCount());
    const auto trackOperator =
            ReloadCoverInfoTrackPointerOperation();
    applyTrackPointerOperation(
            progressLabelText,
            &trackOperator);
}

void WTrackMenu::slotRemove() {
    if (!m_pTrackModel) {
        return;
    }
    m_pTrackModel->removeTracks(getTrackIndices());
    emit restoreCurrentViewStateOrIndex();
}

void WTrackMenu::slotHide() {
    if (!m_pTrackModel) {
        return;
    }
    m_pTrackModel->hideTracks(getTrackIndices());
    emit restoreCurrentViewStateOrIndex();
}

void WTrackMenu::slotUnhide() {
    if (!m_pTrackModel) {
        return;
    }
    m_pTrackModel->unhideTracks(getTrackIndices());
    emit restoreCurrentViewStateOrIndex();
}

void WTrackMenu::slotPurge() {
    if (!m_pTrackModel) {
        return;
    }
    m_pTrackModel->purgeTracks(getTrackIndices());
    emit restoreCurrentViewStateOrIndex();
}

void WTrackMenu::clearTrackSelection() {
    m_pTrack = nullptr;
    m_deckGroup = QString();
    m_trackIndexList.clear();
}

bool WTrackMenu::isCortinaList() const {
    // The Auto DJ queue model is the only one that shows cortina marks, so use
    // that flag to scope the cortina toggle to the Auto DJ list.
    const auto* pTableModel = dynamic_cast<const BaseTrackTableModel*>(m_pTrackModel);
    return pTableModel && pTableModel->showCortinaMarks();
}

bool WTrackMenu::featureIsEnabled(Feature flag) const {
    bool optionIsSelected = m_eActiveFeatures.testFlag(flag);
    if (!optionIsSelected) {
        return false;
    }

    if (!m_pTrackModel) {
        return !m_eTrackModelFeatures.testFlag(flag);
    }

    switch (flag) {
    case Feature::AutoDJ:
        return m_pTrackModel->hasCapabilities(TrackModel::Capability::AddToAutoDJ);
    case Feature::LoadTo:
        return m_pTrackModel->hasCapabilities(
                       TrackModel::Capability::LoadToDeck) ||
                m_pTrackModel->hasCapabilities(
                        TrackModel::Capability::LoadToSampler) ||
                m_pTrackModel->hasCapabilities(
                        TrackModel::Capability::LoadToPreviewDeck);
    case Feature::Playlist:
    case Feature::Crate:
        return m_pTrackModel->hasCapabilities(
                TrackModel::Capability::AddToTrackSet);
    case Feature::Remove:
        return m_pTrackModel->hasCapabilities(
                       TrackModel::Capability::Remove) ||
                m_pTrackModel->hasCapabilities(
                        TrackModel::Capability::RemovePlaylist) ||
                m_pTrackModel->hasCapabilities(
                        TrackModel::Capability::RemoveCrate);
    case Feature::Metadata:
        return m_pTrackModel->hasCapabilities(TrackModel::Capability::EditMetadata);
    case Feature::Analyze:
        return m_pTrackModel->hasCapabilities(
                TrackModel::Capability::EditMetadata |
                TrackModel::Capability::Analyze);
    case Feature::Reset:
        return m_pTrackModel->hasCapabilities(
                TrackModel::Capability::EditMetadata |
                TrackModel::Capability::ResetPlayed);
    case Feature::BPM:
        return m_pTrackModel->hasCapabilities(TrackModel::Capability::EditMetadata);
    case Feature::Color:
        return m_pTrackModel->hasCapabilities(TrackModel::Capability::EditMetadata);
    case Feature::HideUnhidePurge:
        return m_pTrackModel->hasCapabilities(TrackModel::Capability::Hide) ||
                m_pTrackModel->hasCapabilities(TrackModel::Capability::Unhide) ||
                m_pTrackModel->hasCapabilities(TrackModel::Capability::Purge);
    case Feature::RemoveFromDisk:
        return m_pTrackModel->hasCapabilities(TrackModel::Capability::RemoveFromDisk);
    case Feature::FileBrowser:
        return true;
    case Feature::FindOnWeb:
        return true;
    case Feature::Properties:
        return m_pTrackModel->hasCapabilities(TrackModel::Capability::Properties);
    case Feature::SearchRelated:
        return m_pLibrary != nullptr;
    case Feature::SelectInLibrary:
        return m_pTrack != nullptr;
    default:
        DEBUG_ASSERT(!"unreachable");
        return false;
    }
}
