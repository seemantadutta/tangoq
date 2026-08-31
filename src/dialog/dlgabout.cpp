#include "dialog/dlgabout.h"

#include <QDebug>
#include <QEvent>
#include <QFile>
#include <QLocale>
#include <QMouseEvent>
#include <QUrl>

#include "defs_urls.h"
#include "moc_dlgabout.cpp"
#include "util/color/color.h"
#include "util/desktophelper.h"
#include "util/versionstore.h"

DlgAbout::DlgAbout()
        : QDialog(nullptr),
          Ui::DlgAboutDlg() {
    setupUi(this);
    setWindowIcon(QIcon(MIXXX_ICON_PATH));

    mixxx_icon->load(QString(MIXXX_ICON_PATH));
    // The default wordmark's "Tango" is near-white for the dark toolbar and
    // vanishes on a light-themed dialog, so pick the dark-wordmark variant when
    // the dialog background is light. Mirrors the heart-icon choice below.
    const bool darkBackground = Color::isDimColor(palette().window().color());
    mixxx_logo->load(QString(darkBackground ? MIXXX_LOGO_PATH : MIXXX_LOGO_DARK_PATH));

    // Let the wordmark act as a link to the project's own site, the way an About
    // box logo usually does.
    mixxx_logo->setCursor(Qt::PointingHandCursor);
    mixxx_logo->setToolTip(TANGOQ_SUPPORT_URL);
    mixxx_logo->installEventFilter(this);

    version_label->setText(VersionStore::applicationName() +
            QStringLiteral(" version ") + VersionStore::forkVersion());
    git_version_label->setText(VersionStore::gitVersion());
    qt_version_label->setText(VersionStore::qtVersion());
    platform_label->setText(VersionStore::platform());
    QLocale locale;
    date_label->setText(locale.toString(VersionStore::date().toLocalTime(), QLocale::LongFormat));

    QFile licenseFile(":/LICENSE");
    if (!licenseFile.open(QIODevice::ReadOnly)) {
        qWarning() << "LICENSE file not found";
    } else {
        licenseText->setPlainText(licenseFile.readAll());
    }

    QString s_devTeam =
            tr("Mixxx %1.%2 Development Team")
                    .arg(QString::number(
                                 VersionStore::versionNumber().majorVersion()),
                            QString::number(VersionStore::versionNumber()
                                                    .minorVersion()));
    QString s_contributions = tr("With contributions from:");
    QString s_specialThanks = tr("And special thanks to:");
    QString s_pastDevs = tr("Past Developers");
    QString s_pastContribs = tr("Past Contributors");

    QStringList thisReleaseDevelopers;
    thisReleaseDevelopers
            << "RJ Skerry-Ryan"
            << "Owen Williams"
            << "Daniel Sch&uuml;rmann"
            << "S&eacute;bastien Blaisot"
            << "ronso0"
            << "Jan Holthuis"
            << "Nikolaus Einhauser"
            << "Ferran Pujol Camins"
            << "J&ouml;rg Wartenberg"
            << "Fredrik Wieczerkowski"
            << "Maarten de Boer"
            << "Antoine Colombier"
            << "Evelynne Veys";

    // This list should contains all contributors committed
    // code to the Mixxx core within the past two years.
    // New Contributors are added at the end.
    QStringList recentContributors;
    recentContributors
            << "Be"
            << "Uwe Klotz"
            << "D&aacute;vid Szak&aacute;llas"
            << "Philip Gottschling"
            << "Adam Szmigin"
            << "Christian"
            << "Geraldo Nascimento"
            << "Allen Wittenauer"
            << "Raphael Bigal"
            << "Filok"
            << "tcoyvwac"
            << "Tobias Oszlanyi (OsZ)"
            << "Fatih Emre YILDIZ"
            << "Neil Naveen"
            << "Javier Vilalta"
            << "David Chocholat&yacute;"
            << "Fabian Wolter"
            << "Matteo Gheza"
            << "Michael Bacarella"
            << "Bilal Ahmed Karbelkar"
            << "Alice Psykose"
            << "Jakob Leifhelm"
            << "Florian Goth"
            << "Chase Durand"
            << "John Last"
            << "Jakub Kopa&nacute;ko"
            << "Saksham Hans"
            << "Robbert van der Helm"
            << "Andrew Burns"
            << "Michael Wigard"
            << "Alexandre Bique"
            << "Milkii Brewster"
            << "djantti"
            << "Eugene Erokhin"
            << "Ben Duval"
            << "Nicolau Leal Werneck"
            << "David Guglielmi"
            << "Chris H. Meyer"
            << "Mariano Ntrougkas"
            << "Daniel Fernandes"
            << "Gr&eacute;goire Locqueville"
            << "grizeldi"
            << "codingspiderfox"
            << "Ashnidh Khandelwal"
            << "Sergey"
            << "Raphael Quast"
            << "Christophe Henry"
            << "Lukas Waslowski"
            << "Marcin Cie&#x15B;lak" // &#x15B; = &sacute; in HTML 5.0
            << "HorstBaerbel"
            << "gqzomer"
            << "Bacadam"
            << "Leon Eckardt"
            << "Th&eacute;odore Noel"
            << "Aquassaut"
            << "Morgan Nunan"
            << "FrankwaP"
            << "Markus Kohlhase"
            << "Daniel Fernandes"
            << "Frank Grimy"
            << "Al Hadebe"
            << "Emilien Colombier"
            << "DJ aK"
            << "Sam Whited"
            << "Ryan Bell"
            << "Nicolas Parlant"
            << "Ralf Pachali"
            << "Patrick Taels"
            << "armaan"
            << "Karam Assany"
            << "Anmol Mishra"
            << "Alec Peng"
            << "Arthur Vimond"
            << "Johan Jnn"
            << "Shiraz McClennon"
            << "Hetarth Jodha"
            << "Harsh Barhate"
            << "Nikhil Bisht"
            << "Jan Claußen"
            << "Nisarg Shah"
            << "Manish Sehrawat"
            << "xuijuthub"
            << "Nicolay Leiva"
            << "Didier Malenfant"
            << "Rene E"
            << "Owen Turnbull"
            << "Benjamin Paker"
            << "J&eacute;r&ocirc;me Froissart"
            << "Harshit Singh Hada"
            << "Andrej Lajovic"
            << "Pri-yan-shu"
            << "Graham Hall"
            << "Ayush Sah"
            << "fixxiefixx"
            << "evoixmr"
            << "Greg-Orca"
            << "Iron-Wolf"
            << "endcredits33"
            << "cucucat"
            << "Laura Mora";

    QStringList specialThanks;
    specialThanks
            << "Mark Hills"
            << "Oscillicious"
            << "Vestax"
            << "Stanton"
            << "Hercules"
            << "EKS"
            << "Echo Digital Audio"
            << "JP Disco"
            << "Google Summer of Code"
            << "Adam Bellinson"
            << "Alexandre Bancel"
            << "Melanie Thielker"
            << "Julien Rosener"
            << "Pau Arum&iacute;"
            << "David Garcia"
            << "Seb Ruiz"
            << "Joseph Mattiello";

    QStringList pastDevelopers;
    pastDevelopers
            << "Tue Haste Andersen"
            << "Ken Haste Andersen"
            << "Cedric Gestes"
            << "John Sully"
            << "Torben Hohn"
            << "Peter Chang"
            << "Micah Lee"
            << "Ben Wheeler"
            << "Wesley Stessens"
            << "Nathan Prado"
            << "Zach Elko"
            << "Tom Care"
            << "Pawel Bartkiewicz"
            << "Nick Guenther"
            << "Adam Davison"
            << "Garth Dahlstrom"
            << "Albert Santoni"
            << "Phillip Whelan"
            << "Tobias Rafreider"
            << "Bill Good"
            << "Vittorio Colao"
            << "Thomas Vincent"
            << "Ilkka Tuohela"
            << "Max Linke"
            << "Marcos Cardinot"
            << "Nicu Badescu"
            << "Uwe Klotz"
            << "Sean Pappalardo"
            << "S. Brandt";

    QStringList pastContributors;
    pastContributors
            << "Ludek Hor&#225;cek"
            << "Svein Magne Bang"
            << "Kristoffer Jensen"
            << "Ingo Kossyk"
            << "Mads Holm"
            << "Lukas Zapletal"
            << "Jeremie Zimmermann"
            << "Gianluca Romanin"
            << "Tim Jackson"
            << "Stefan Langhammer"
            << "Frank Willascheck"
            << "Jeff Nelson"
            << "Kevin Schaper"
            << "Alex Markley"
            << "Oriol Puigb&oacute;"
            << "Ulrich Heske"
            << "James Hagerman"
            << "quil0m80"
            << "Martin Sakm&#225;r"
            << "Ilian Persson"
            << "Dave Jarvis"
            << "Thomas Baag"
            << "Karlis Kalnins"
            << "Amias Channer"
            << "Sacha Berger"
            << "James Evans"
            << "Martin Sakmar"
            << "Navaho Gunleg"
            << "Gavin Pryke"
            << "Michael Pujos"
            << "Claudio Bantaloukas"
            << "Pavol Rusnak"
            << "Bruno Buccolo"
            << "Ryan Baker"
            << "Andre Roth"
            << "Robin Sheat"
            << "Mark Glines"
            << "Mathieu Rene"
            << "Miko Kiiski"
            << "Brian Jackson"
            << "Andreas Pflug"
            << "Bas van Schaik"
            << "J&aacute;n Jockusch"
            << "Oliver St&ouml;neberg"
            << "Jan Jockusch"
            << "C. Stewart"
            << "Bill Egert"
            << "Zach Shutters"
            << "Owen Bullock"
            << "Graeme Mathieson"
            << "Sebastian Actist"
            << "Jussi Sainio"
            << "David Gnedt"
            << "Antonio Passamani"
            << "Guy Martin"
            << "Anders Gunnarsson"
            << "Mikko Jania"
            << "Juan Pedro Bol&iacute;var Puente"
            << "Linus Amvall"
            << "Irwin C&eacute;spedes B"
            << "Micz Flor"
            << "Daniel James"
            << "Mika Haulo"
            << "Tom Mast"
            << "Miko Kiiski"
            << "Vin&iacute;cius Dias dos Santos"
            << "Joe Colosimo"
            << "Shashank Kumar"
            << "Till Hofmann"
            << "Peter V&aacute;gner"
            << "Jens Nachtigall"
            << "Scott Ullrich"
            << "Jonas &Aring;dahl"
            << "Jonathan Costers"
            << "Maxime Bochon"
            << "Akash Shetye"
            << "Pascal Bleser"
            << "Florian Mahlknecht"
            << "Ben Clark"
            << "Tom Gascoigne"
            << "Aaron Mavrinac"
            << "Markus H&auml;rer"
            << "Scott Stewart"
            << "Nimatek"
            << "Matthew Mikolay"
            << "Thanasis Liappis"
            << "Daniel Lindenfelser"
            << "Andrey Smelov"
            << "Alban Bedel"
            << "Steven Boswell"
            << "Jo&atilde;o Reys Santos"
            << "Carl Pillot"
            << "Vedant Agarwala"
            << "Nazar Gerasymchuk"
            << "Federico Briata"
            << "Leo Combes"
            << "Florian Kiekh&auml;fer"
            << "Michael Sawyer"
            << "Quentin Faidide"
            << "Peter G. Marczis"
            << "Khyrul Bashar"
            << "Johannes Obermayr"
            << "Kevin Lee"
            << "Evan Radkoff"
            << "Lee Matos"
            << "Ryan Kramer"
            << "Zak Reynolds"
            << "Dennis Rohner"
            << "Juha Pitk&auml;nen"
            << "Varun Jewalikar"
            << "Dennis Wallace"
            << "Keith Salisbury"
            << "Irina Grosu"
            << "Callum Styan"
            << "Rahul Behl"
            << "Markus Baertschi"
            << "Don Dennis"
            << "Alexandru Jercaianu"
            << "Nils Goroll"
            << "Marco Angerer"
            << "Thorsten Munsch"
            << "Emile Vrijdags"
            << "St&eacute;phane Guillou"
            << "Russ Mannex"
            << "Brendan Austin"
            << "Lorenz Drescher"
            << "James Atwill"
            << "Alex Barker"
            << "Jean Claveau"
            << "Kevin Wern"
            << "Vladim&iacute;r Dudr"
            << "Neale Pickett"
            << "Chlo&eacute; Avrillon"
            << "Hendrik Reglin"
            << "Serge Ukolov"
            << "Patric Schmitz"
            << "Roland Schwarz"
            << "Jan Ypma"
            << "Andreas M&uuml;ller"
            << "Sam Cross"
            << "Joey Pabalinas"
            << "Markus Kl&ouml;sges"
            << "Pavel Potocek"
            << "Timothy Rae"
            << "Leigh Scott"
            << "William Lemus"
            << "Nimit Bhardwaj"
            << "Pavel Sokolov"
            << "Devananda van der Veen"
            << "Tatsuyuki Ishi"
            << "Kilian Feess"
            << "Conner Phillips"
            << "Artyom Lyan"
            << "Johan Lasperas"
            << "Olaf Hering"
            << "Eduardo Acero"
            << "Thomas Jarosch"
            << "Nico Schl&ouml;mer"
            << "Joan Marc&egrave; i Igual"
            << "Stefan Weber"
            << "Matthew Nicholson"
            << "Jamie Gifford"
            << "Sebastian Reu&szlig;e"
            << "Pawe&#322; Goli&#324;ski"
            << "beenisss"
            << "Tuukka Pasanen"
            << "Josep Maria Antol&iacute;n Segura"
            << "St&eacute;phane Lepin"
            << "Bernd Binder"
            << "Pradyuman"
            << "Nik Martin"
            << "Kerrick Staley"
            << "Raphael Graf"
            << "YunQiang Su"
            << "Melissa"
            << "Ned Haughton"
            << "Cristiano Lacerda"
            << "Ketan Lambat"
            << "Edward Kigwana"
            << "Simon Harst"
            << "J&eacute;r&ocirc;me Blanchi"
            << "Chris Hills"
            << "David Lowenfels"
            << "Matthieu Bouron"
            << "Nathan Korth"
            << "Edward Millen"
            << "Frank Breitling"
            << "Albert Aparicio"
            << "Pierre Le Gall"
            << "David Baker"
            << "Justin Kourie"
            << "Waylon Robertson"
            << "Ball&oacute; Gy&ouml;rgy"
            << "Pino Toscano"
            << "Alexander Horner"
            << "Michael Ehlen"
            << "Alice Midori"
            << "h67ma"
            << "Vincent Duez-Dellac"
            << "Somesh Metri"
            << "Doteya"
            << "olafklingt"
            << "Nino MP"
            << "Daniel Poelzleithner"
            << "luzpaz"
            << "Sebastian Hasler"
            << "Kshitij Gupta"
            << "Evan Dekker"
            << "Harshit Maurya"
            << "Janek Fischer"
            << "Matthias Beyer"
            << "Kristiyan Katsarov"
            << "Sanskar Bajpai"
            << "Javier Vilarroig"
            << "Gary Tunstall"
            << "Viktor Gal"
            << "Maty&aacute;&scaron; Bobek"
            << "Mr. Rincewind"
            << "Stefan N&uuml;rnberger"
            << "motific";

    QString sectionTemplate = QString(
        "<p align=\"center\"><b>%1</b></p><p align=\"center\">%2</p>");
    QStringList sections;
    // TangoQ is a fork. Say so before Mixxx's own credits, so it is never
    // mistaken for the official application and nobody brings TangoQ's bugs
    // to the Mixxx project. The disclaimer matters more than the attribution:
    // the GPL lets us ship modified Mixxx, it does not let us imply endorsement.
    // Everything below this section is upstream's, listed unaltered.
    sections << QString("<p align=\"center\"><b>%1</b></p>"
                        "<p align=\"center\">%2</p>")
                        .arg(tr("About this build"),
                                tr("TangoQ is Argentine Tango DJ software based "
                                   "on Mixxx. It is not affiliated with, "
                                   "supported by, or endorsed by the Mixxx "
                                   "project.<br>"
                                   "Please report problems with TangoQ to its "
                                   "own issue tracker rather than to Mixxx.<br><br>"
                                   "The High Contrast skin color palette draws "
                                   "from the <a href=\"https://github.com/"
                                   "DjAnth0n1/LateNight_Sunrise-Color-Scheme\">"
                                   "LateNight Sunrise Color Scheme</a> by "
                                   "Dj.Anth0n1.<br><br>"
                                   "TangoQ is built on the work of the Mixxx "
                                   "developers and everyone credited below. The "
                                   "original application is at "
                                   "<a href=\"%1\">mixxx.org</a> &mdash; please "
                                   "consider supporting them with a "
                                   "<a href=\"%2\">donation to Mixxx</a>.<br><br>"
                                   "Like Mixxx, TangoQ is free software, "
                                   "distributed under the same terms: the GNU "
                                   "General Public License, version 2 or later. "
                                   "The full license text is in the License tab."
                                   "<br><br>"
                                   "TangoQ fork changes Copyright &copy; 2026 "
                                   "Seemanta Dutta (TangoQ). Mixxx is Copyright "
                                   "&copy; 2001&ndash;2026 the Mixxx Development "
                                   "Team.")
                                        .arg(MIXXX_WEBSITE_URL, MIXXX_DONATE_URL))
             << sectionTemplate.arg(s_devTeam,
                        thisReleaseDevelopers.join("<br>"))
             << sectionTemplate.arg(s_contributions,
                        recentContributors.join("<br>"))
             << sectionTemplate.arg(s_pastDevs,
                        pastDevelopers.join("<br>"))
             << sectionTemplate.arg(s_pastContribs,
                        pastContributors.join("<br>"))
             << sectionTemplate.arg(s_specialThanks,
                        specialThanks.join("<br>"));
    // The credits carry the Mixxx site and donation links, so anchors must be
    // clickable - the .ui sets NoTextInteraction - and open in the browser.
    textBrowser->setTextInteractionFlags(Qt::TextBrowserInteraction);
    textBrowser->setOpenExternalLinks(true);
    textBrowser->setHtml(sections.join(""));

    // Make the fork maintainer contactable, in the dialog's own link colour so it
    // matches the website link rather than defaulting to a raw blue.
    fork_maintainer_label->setTextFormat(Qt::RichText);
    fork_maintainer_label->setOpenExternalLinks(true);
    fork_maintainer_label->setText(
            tr("TangoQ Maintainer: "
               "<a style=\"color:%1;\" href=\"mailto:seemanta@gmail.com\">Seemanta Dutta</a>")
                    .arg(Color::blendColors(palette().link().color(),
                            palette().text().color())
                                    .name()));

    // One support button, for TangoQ. Mixxx's own site and donation links live in
    // the credits text above, so a good-faith supporter of either project can
    // reach the right place. The "Official Website" label and the separate
    // "Donate to Mixxx" button were removed in favour of that. The heart icon
    // (with the occasional rainbow) rides on this button now.
    if (std::rand() % 6) {
        if (!Color::isDimColor(palette().text().color())) {
            btnSupportFork->setIcon(QIcon(":/images/heart_icon_light.svg"));
        } else {
            btnSupportFork->setIcon(QIcon(":/images/heart_icon_dark.svg"));
        }
    } else {
        btnSupportFork->setIcon(QIcon(":/images/heart_icon_rainbow.svg"));
    }
    btnSupportFork->setText(tr("Support TangoQ"));
    connect(btnSupportFork, &QPushButton::clicked, this, [] {
        mixxx::DesktopHelper::openUrl(QUrl(TANGOQ_SUPPORT_URL));
    });

    connect(buttonBox, &QDialogButtonBox::accepted, this, &DlgAbout::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &DlgAbout::reject);
}

bool DlgAbout::eventFilter(QObject* pObject, QEvent* pEvent) {
    // Release rather than press, so dragging off the logo cancels the click the
    // way a button would.
    if (pObject == mixxx_logo && pEvent->type() == QEvent::MouseButtonRelease) {
        const auto* pMouseEvent = static_cast<QMouseEvent*>(pEvent);
        if (pMouseEvent->button() == Qt::LeftButton &&
                mixxx_logo->rect().contains(pMouseEvent->pos())) {
            mixxx::DesktopHelper::openUrl(QUrl(TANGOQ_SUPPORT_URL));
            return true;
        }
    }
    return QDialog::eventFilter(pObject, pEvent);
}
