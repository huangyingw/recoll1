/* Copyright (C) 2005 J.F.Dockes
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the
 *   Free Software Foundation, Inc.,
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */
#include "autoconfig.h"

#include <QMessageBox>
#include <QTimer>

#include "execmd.h"
#include "debuglog.h"
#include "transcode.h"
#include "indexer.h"
#include "rclmain_w.h"

using namespace std;

void RclMain::idxStatus()
{
    ConfSimple cs(theconfig->getIdxStatusFile().c_str(), 1);
    QString msg = tr("Indexing in progress: ");
    DbIxStatus status;
    string val;
    cs.get("phase", val);
    status.phase = DbIxStatus::Phase(atoi(val.c_str()));
    cs.get("fn", status.fn);
    cs.get("docsdone", val);
    status.docsdone = atoi(val.c_str());
    cs.get("filesdone", val);
    status.filesdone = atoi(val.c_str());
    cs.get("dbtotdocs", val);
    status.dbtotdocs = atoi(val.c_str());

    QString phs;
    switch (status.phase) {
    case DbIxStatus::DBIXS_NONE:phs=tr("None");break;
    case DbIxStatus::DBIXS_FILES: phs=tr("Updating");break;
    case DbIxStatus::DBIXS_PURGE: phs=tr("Purge");break;
    case DbIxStatus::DBIXS_STEMDB: phs=tr("Stemdb");break;
    case DbIxStatus::DBIXS_CLOSING:phs=tr("Closing");break;
    case DbIxStatus::DBIXS_DONE:phs=tr("Done");break;
    case DbIxStatus::DBIXS_MONITOR:phs=tr("Monitor");break;
    default: phs=tr("Unknown");break;
    }
    msg += phs + " ";
    if (status.phase == DbIxStatus::DBIXS_FILES) {
	char cnts[100];
	if (status.dbtotdocs > 0)
	    sprintf(cnts,"(%d/%d/%d) ", status.docsdone, status.filesdone, 
		    status.dbtotdocs);
	else
	    sprintf(cnts, "(%d/%d) ", status.docsdone, status.filesdone);
	msg += QString::fromUtf8(cnts) + " ";
    }
    string mf;int ecnt = 0;
    string fcharset = theconfig->getDefCharset(true);
    if (!transcode(status.fn, mf, fcharset, "UTF-8", &ecnt) || ecnt) {
	mf = url_encode(status.fn, 0);
    }
    msg += QString::fromUtf8(mf.c_str());
    statusBar()->showMessage(msg, 4000);
}

// This is called by a periodic timer to check the status of 
// indexing, a possible need to exit, and cleanup exited viewers
void RclMain::periodic100()
{
    LOGDEB2(("Periodic100\n"));
    if (m_idxproc) {
	// An indexing process was launched. If its' done, see status.
	int status;
	bool exited = m_idxproc->maybereap(&status);
	if (exited) {
	    deleteZ(m_idxproc);
	    if (status) {
                if (m_idxkilled) {
                    QMessageBox::warning(0, "Recoll", 
                                         tr("Indexing interrupted"));
                    m_idxkilled = false;
                } else {
                    QMessageBox::warning(0, "Recoll", 
                                         tr("Indexing failed"));
                }
	    } else {
		// On the first run, show missing helpers. We only do this once
		if (m_firstIndexing)
		    showMissingHelpers();
	    }
	    string reason;
	    maybeOpenDb(reason, 1);
	} else {
	    // update/show status even if the status file did not
	    // change (else the status line goes blank during
	    // lengthy operations).
	    idxStatus();
	}
    }
    // Update the "start/stop indexing" menu entry, can't be done from
    // the "start/stop indexing" slot itself
    IndexerState prevstate = m_indexerState;
    if (m_idxproc) {
	m_indexerState = IXST_RUNNINGMINE;
	fileToggleIndexingAction->setText(tr("Stop &Indexing"));
	fileToggleIndexingAction->setEnabled(true);
        fileRetryFailedAction->setEnabled(false);
	fileRebuildIndexAction->setEnabled(false);
	periodictimer->setInterval(200);
    } else {
	Pidfile pidfile(theconfig->getPidfile());
	if (pidfile.open() == 0) {
	    m_indexerState = IXST_NOTRUNNING;
	    fileToggleIndexingAction->setText(tr("Update &Index"));
            fileRetryFailedAction->setEnabled(true);
	    fileToggleIndexingAction->setEnabled(true);
	    fileRebuildIndexAction->setEnabled(true);
	    periodictimer->setInterval(1000);
	} else {
	    // Real time or externally started batch indexer running
	    m_indexerState = IXST_RUNNINGNOTMINE;
	    fileToggleIndexingAction->setText(tr("Stop &Indexing"));
	    fileToggleIndexingAction->setEnabled(true);
            fileRetryFailedAction->setEnabled(false);
	    fileRebuildIndexAction->setEnabled(false);
	    periodictimer->setInterval(200);
	}	    
    }

    if ((prevstate == IXST_RUNNINGMINE || prevstate == IXST_RUNNINGNOTMINE)
        && m_indexerState == IXST_NOTRUNNING) {
        showTrayMessage("Indexing done");
    }

    // Possibly cleanup the dead viewers
    for (vector<ExecCmd*>::iterator it = m_viewers.begin();
	 it != m_viewers.end(); it++) {
	int status;
	if ((*it)->maybereap(&status)) {
	    deleteZ(*it);
	}
    }
    vector<ExecCmd*> v;
    for (vector<ExecCmd*>::iterator it = m_viewers.begin();
	 it != m_viewers.end(); it++) {
	if (*it)
	    v.push_back(*it);
    }
    m_viewers = v;

    if (recollNeedsExit)
	fileExit();
}

// This gets called when the "update index" action is activated. It executes
// the requested action, and disables the menu entry. This will be
// re-enabled by the indexing status check
void RclMain::toggleIndexing()
{
    switch (m_indexerState) {
    case IXST_RUNNINGMINE:
	if (m_idxproc) {
	    // Indexing was in progress, request stop. Let the periodic
	    // routine check for the results.
	    int pid = m_idxproc->getChildPid();
	    if (pid > 0) {
		kill(pid, SIGTERM);
                m_idxkilled = true;
            }
	}
	break;
    case IXST_RUNNINGNOTMINE:
    {
	int rep = 
	    QMessageBox::information(0, tr("Warning"), 
				     tr("The current indexing process "
					"was not started from this "
					"interface. Click Ok to kill it "
					"anyway, or Cancel to leave it alone"),
					 QMessageBox::Ok,
					 QMessageBox::Cancel,
					 QMessageBox::NoButton);
	if (rep == QMessageBox::Ok) {
	    Pidfile pidfile(theconfig->getPidfile());
	    pid_t pid = pidfile.open();
	    if (pid > 0)
		kill(pid, SIGTERM);
	}
    }		
    break;
    case IXST_NOTRUNNING:
    {
	// Could also mean that no helpers are missing, but then we
	// won't try to show a message anyway (which is what
	// firstIndexing is used for)
	string mhd;
	m_firstIndexing = !theconfig->getMissingHelperDesc(mhd);

	vector<string> args;

        string badpaths;
        args.push_back("recollindex");
        args.push_back("-E");
        ExecCmd::backtick(args, badpaths);
        if (!badpaths.empty()) {
            int rep = 
                QMessageBox::warning(0, tr("Bad paths"), 
				     tr("Bad paths in configuration file:\n") +
                                     QString::fromLocal8Bit(badpaths.c_str()),
                                     QMessageBox::Ok,
                                     QMessageBox::Cancel,
                                     QMessageBox::NoButton);
            if (rep == QMessageBox::Cancel)
                return;
        }

        args.clear();
	args.push_back("-c");
	args.push_back(theconfig->getConfDir());
        if (fileRetryFailedAction->isChecked())
            args.push_back("-k");
	m_idxproc = new ExecCmd;
	m_idxproc->startExec("recollindex", args, false, false);
    }
    break;
    case IXST_UNKNOWN:
        return;
    }
}

void RclMain::rebuildIndex()
{
    switch (m_indexerState) {
    case IXST_UNKNOWN:
    case IXST_RUNNINGMINE:
    case IXST_RUNNINGNOTMINE:
	return; //?? Should not have been called
    case IXST_NOTRUNNING:
    {
	int rep = 
	    QMessageBox::warning(0, tr("Erasing index"), 
				     tr("Reset the index and start "
					"from scratch ?"),
					 QMessageBox::Ok,
					 QMessageBox::Cancel,
					 QMessageBox::NoButton);
	if (rep == QMessageBox::Ok) {
	    // Could also mean that no helpers are missing, but then we
	    // won't try to show a message anyway (which is what
	    // firstIndexing is used for)
	    string mhd;
	    m_firstIndexing = !theconfig->getMissingHelperDesc(mhd);
	    vector<string> args;
	    args.push_back("-c");
	    args.push_back(theconfig->getConfDir());
	    args.push_back("-z");
	    m_idxproc = new ExecCmd;
	    m_idxproc->startExec("recollindex", args, false, false);
	}
    }
    break;
    }
}

void RclMain::updateIdxForDocs(vector<Rcl::Doc>& docs)
{
    if (m_idxproc) {
	QMessageBox::warning(0, tr("Warning"), 
			     tr("Can't update index: indexer running"),
			     QMessageBox::Ok, 
			     QMessageBox::NoButton);
	return;
    }
	
    vector<string> paths;
    if (ConfIndexer::docsToPaths(docs, paths)) {
	vector<string> args;
	args.push_back("-c");
	args.push_back(theconfig->getConfDir());
	args.push_back("-e");
	args.push_back("-i");
	args.insert(args.end(), paths.begin(), paths.end());
	m_idxproc = new ExecCmd;
	m_idxproc->startExec("recollindex", args, false, false);
	fileToggleIndexingAction->setText(tr("Stop &Indexing"));
    }
    fileToggleIndexingAction->setEnabled(false);
    fileRetryFailedAction->setEnabled(false);
}
