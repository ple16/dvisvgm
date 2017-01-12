/*************************************************************************
** EPSToSVG.cpp                                                         **
**                                                                      **
** This file is part of dvisvgm -- a fast DVI to SVG converter          **
** Copyright (C) 2005-2017 Martin Gieseking <martin.gieseking@uos.de>   **
**                                                                      **
** This program is free software; you can redistribute it and/or        **
** modify it under the terms of the GNU General Public License as       **
** published by the Free Software Foundation; either version 3 of       **
** the License, or (at your option) any later version.                  **
**                                                                      **
** This program is distributed in the hope that it will be useful, but  **
** WITHOUT ANY WARRANTY; without even the implied warranty of           **
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the         **
** GNU General Public License for more details.                         **
**                                                                      **
** You should have received a copy of the GNU General Public License    **
** along with this program; if not, see <http://www.gnu.org/licenses/>. **
*************************************************************************/

#include <config.h>
#include <fstream>
#include <sstream>
#include "EPSFile.hpp"
#include "EPSToSVG.hpp"
#include "Message.hpp"
#include "MessageException.hpp"
#include "PsSpecialHandler.hpp"
#include "SVGOutput.hpp"
#include "System.hpp"


using namespace std;


void EPSToSVG::convert () {
#ifndef HAVE_LIBGS
	if (!Ghostscript().available())
		throw MessageException("Ghostscript is required to process the EPS file");
#endif
	EPSFile epsfile(_fname);
	if (!epsfile.hasValidHeader())
		throw PSException("invalid EPS file");

	BoundingBox bbox;
	epsfile.bbox(bbox);
	if (bbox.width() == 0 || bbox.height() == 0)
		Message::wstream(true) << "bounding box of file " << _fname << " is empty\n";
	Message::mstream(false, Message::MC_PAGE_NUMBER) << "processing file " << _fname << '\n';
	Message::mstream().indent(1);
	_svg.newPage(1);
	// create a psfile special and forward it to the PsSpecialHandler
	stringstream ss;
	ss << "\"" << _fname << "\" "
			"llx=" << bbox.minX() << " "
			"lly=" << bbox.minY() << " "
			"urx=" << bbox.maxX() << " "
			"ury=" << bbox.maxY();
	try {
		PsSpecialHandler pshandler;
		pshandler.process("psfile=", ss, *this);
	}
	catch (...) {
		progress(0);  // remove progress message
		throw;
	}
	progress(0);
	// output SVG file
	_svg.setBBox(_bbox);
	_svg.appendToDoc(new XMLCommentNode(" This file was generated by dvisvgm " VERSION " "));
	bool success = _svg.write(_out.getPageStream(1, 1));
	string svgfname = _out.filename(1, 1);
	if (svgfname.empty())
		svgfname = "<stdout>";
	if (!success)
		Message::wstream() << "failed to write output to " << svgfname << '\n';
	else {
		const double bp2pt = 72.27/72;
		const double bp2mm = 25.4/72;
		Message::mstream(false, Message::MC_PAGE_SIZE) << "graphic size: " << XMLString(bbox.width()*bp2pt) << "pt"
			" x " << XMLString(bbox.height()*bp2pt) << "pt"
			" (" << XMLString(bbox.width()*bp2mm) << "mm"
			" x " << XMLString(bbox.height()*bp2mm) << "mm)\n";
		Message::mstream(false, Message::MC_PAGE_WRITTEN) << "output written to " << svgfname << '\n';
	}
}


string EPSToSVG::getSVGFilename (unsigned pageno) const {
	if (pageno == 1)
		return _out.filename(1, 1);
	return "";
}


void EPSToSVG::progress (const char *id) {
	static double time=0;
	static bool draw=false; // show progress indicator?
	static size_t count=0;
	count++;
	// don't show the progress indicator before the given time has elapsed
	if (!draw && System::time()-time > PROGRESSBAR_DELAY) {
		draw = true;
		Terminal::cursor(false);
		Message::mstream(false) << "\n";
	}
	if (draw && ((System::time() - time > 0.05) || id == 0)) {
		const size_t DIGITS=6;
		Message::mstream(false, Message::MC_PROGRESS)
			<< string(DIGITS-min(DIGITS, static_cast<size_t>(log10(count))), ' ')
			<< count << " PostScript instructions processed\r";
		// overprint indicator when finished
		if (id == 0) {
			Message::estream().clearline();
			Terminal::cursor(true);
		}
		time = System::time();
	}
}
