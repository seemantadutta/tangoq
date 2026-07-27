# Fixed

* Lit dancer icon is hard to make out in ON and OFF state, maybe make it red in color.
* Dancer icon should disappear in non tango mode, so that plain mixxx does not show this behavior. So the fix then becomes showing a red couple in the icon in tango mode, removing it completely when in non tango mode
* AutoDJ reset should also eject the decks so that we can start with a truly clean slate and maybe the menu option should be called 'Eject decks and reset AutoDJ queue state'
* autoDJ should disable AFTER last track has finished playing, not when the last track starts to play
* Double clicking on a track during LIVE mode should not replace tracks on any deck
* Add to Auto DJ (top) and (replace) were only blocked in the track menu; the double-click action, the controller controls and the playlist/external-library sidebars could still reorder or wipe a Tango queue
* Set the cortina tag in the deck area too, not just in the auto DJ list - fixed
* Add keyboard shortcut for tangomode on/off with same the gates today which allows/disallows setting tango mode on/off (when autoDJ is on) - fixed
* Rename mixxx.exe target to tangomode.exe - fixed
* Support drag and drop to autodj docking/floating window
* Detect duplicates and show warnig, but ignore cortinas when adding them as duplicates (maybe color code them or add a DUPLICATE cortina tag?)
* Message at shutdown: warning [LibraryScanner 1] QSqlDatabasePrivate::removeDatabase: connection 'MIXXX-2' is still in use, all queries will cease to work.
* Crash on quit in Tango mode with tracks on the decks. Symptom seen first as "the Auto DJ queue comes up with played rows still greyed from the previous run" - the played flags are only cleared by TrackDAO::finish(), which runs on the clean shutdown path, so a crash silently keeps them. The crash itself: PlayerManager is destroyed before the Library that owns AutoDJProcessor, and a deck's destructor emits PlayerInfo::trackChanged from unloadTrack(), which we answered inline by walking every deck - reading one that was already freed.
* Added mechanism for pause after arbitrary track (cortinas included), plus a session display name for any track. Between them these cover announcements, performance tracks and intro/outro blocks: a performance is a track you rename and mark, an intro block is one you put a mark after. A red separator line in the list shows where the set will stop. The four track types (milonga/intro/outro/performance) were tried first and removed - they asked you to declare the shape of the set up front, and the set is actually built on the fly during the milonga.
----

# Low priority today
* Implement 'inline' preview on cue end point, which can work by clicking a single button next to the track, in the track listing or even in the autoDJ list. - this is low priority, mixxx already supports this
* When re-launching mixxx, it loses the cortina designation of all tracks that were added as cortinas. Can we implement a system where it can be restored, but I also don't want to permanently tag a track as a cortina. Mistakenly tagging a normal track as a cortina will then apply a fade by mistake. Unless we lock the fading controls in the 'tracks' between two cortinas.


----

# Long term features

* Detect tracks with big gaps or audio cut outs
* configurable tango mode toolbar so that I can choose what I want to see, like milonga under/over times etc.
* Tanda suggestion window - needs more work and needs a well tagged library. Maybe add a system to tag tracks as popular/unpopular during a live gig and then build the data set over time which can then be used by AI/ML algorithms to generate tanda suggestions based on the expected dancer outcomes. The data set building can happen over several months to years.
