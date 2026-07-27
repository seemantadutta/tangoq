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
* Added mechanism for pause after arbitrary track, that helps with announcements, added performance track and intro/outro track functionality
----

# Medium effort
* Add a intro-outro list, where tracks can be enqueued to serve as intro music/outtro music and also sound check at the venue. But the transition from intro to main set and from main set to outtro set will be manually done. After intro set ends, main set should not start automatically. And after main set ends, outtro set should not start automatically either.
* Add a separate 'ANNOUNCEMENT' tag for a pseudotrack that does nothing but stop the autoDJ to allow for organizers to make announcements. This way I can set it up right after the cortina, and next tanda won't play. But it will stop at that point. And I should be able to hit 'continue' or some control to carry on with the next tanda. This way I don't have to manually insert 5min silence tracks when the organizer wants me to stop the DJ flow and continue after the announcements.
* Add a separate list, which will contain tracks to be used during a performance. In performance mode, each track should stop after playing, so that audience can applaud, clap etc. And the DJ has to manually start each track, not a back to back play, which happens in a normal autoDJ list

# Low priority today
* Implement 'inline' preview on cue end point, which can work by clicking a single button next to the track, in the track listing or even in the autoDJ list. - this is low priority, mixxx already supports this
* When re-launching mixxx, it loses the cortina designation of all tracks that were added as cortinas. Can we implement a system where it can be restored, but I also don't want to permanently tag a track as a cortina. Mistakenly tagging a normal track as a cortina will then apply a fade by mistake. Unless we lock the fading controls in the 'tracks' between two cortinas.


----

# Long term features

* Detect tracks with big gaps or audio cut outs
* configurable tango mode toolbar so that I can choose what I want to see, like milonga under/over times etc.
* Tanda suggestion window - needs more work and needs a well tagged library. Maybe add a system to tag tracks as popular/unpopular during a live gig and then build the data set over time which can then be used by AI/ML algorithms to generate tanda suggestions based on the expected dancer outcomes. The data set building can happen over several months to years.
