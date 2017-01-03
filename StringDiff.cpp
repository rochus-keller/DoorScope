/*
* Copyright 2005-2017 Rochus Keller <mailto:me@rochus-keller.info>
*
* This file is part of the DoorScope application
* see <http://doorscope.ch/>).
*
* GNU General Public License Usage
* This file may be used under the terms of the GNU General Public
* License (GPL) versions 2.0 or 3.0 as published by the Free Software
* Foundation and appearing in the file LICENSE.GPL included in
* the packaging of this file. Please review the following information
* to ensure GNU General Public Licensing requirements will be met:
* http://www.fsf.org/licensing/licenses/info/GPLv2.html and
* http://www.gnu.org/copyleft/gpl.html.
*/

// Quelle: http://www.mathertel.de/Diff/DiffTextLines.aspx
/*
Copyright (c) 2005-2012 by Matthias Hertel, http://www.mathertel.de/
All rights reserved.
Redistribution and use in source and binary forms, with or without modification, are permitted provided
that the following conditions are met:
Redistributions of source code must retain the above copyright notice, this list of conditions and the
following disclaimer.
Redistributions in binary form must reproduce the above copyright notice, this list of conditions and
the following disclaimer in the documentation and/or other materials provided with the distribution.
Neither the name of the copyright owners nor the names of its contributors may be used to endorse or
promote products derived from this software without specific prior written permission.
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
  */

#include "StringDiff.h"
#include <QStringList>
#include <QRegExp>
#include <exception>
#include <QtDebug>
using namespace Ds;

class _MyException : public std::exception
{
	QByteArray d_msg;
public:
	_MyException( const char* msg = "" ):d_msg(msg) {}
	~_MyException() throw() {}
	const char* what() const throw() { return d_msg; }
};


QVector<StringDiff::Item> StringDiff::Diff(const QString& ArrayA, const QString& ArrayB)
{
	// The A-Version of the data (original data) to be compared.
	DiffData DataA(ArrayA);

	// The B-Version of the data (modified data) to be compared.
	DiffData DataB(ArrayB);

	const int MAX = DataA.Length + DataB.Length + 1;
	/// vector for the (0,0) to (x,y) search
	QVector<int> DownVector( 2 * MAX + 2 );
	/// vector for the (u,v) to (N,M) search
	QVector<int> UpVector( 2 * MAX + 2 );

	LCS( DataA, 0, DataA.Length, DataB, 0, DataB.Length, DownVector, UpVector );
	return CreateDiffs( DataA, DataB );
}

StringDiff::SMSRD StringDiff::SMS(DiffData& DataA, int LowerA, int UpperA, DiffData& DataB, int LowerB, int UpperB,
								  QVector<int>& DownVector, QVector<int>& UpVector)
{
	SMSRD ret;
	const int MAX = DataA.Length + DataB.Length + 1;

	int DownK = LowerA - LowerB; // the k-line to start the forward search
	int UpK = UpperA - UpperB; // the k-line to start the reverse search

	int Delta = (UpperA - LowerA) - (UpperB - LowerB);
	bool oddDelta = (Delta & 1) != 0;

	// The vectors in the publication accepts negative indexes. the vectors implemented here are 0-based
	// and are access using a specific offset: UpOffset UpVector and DownOffset for DownVektor
	int DownOffset = MAX - DownK;
	int UpOffset = MAX - UpK;

	int MaxD = ((UpperA - LowerA + UpperB - LowerB) / 2) + 1;

	// Debug.Write(2, "SMS", String.Format("Search the box: A[{0}-{1}] to B[{2}-{3}]", LowerA, UpperA, LowerB, UpperB));

	// init vectors
	DownVector[DownOffset + DownK + 1] = LowerA;
	UpVector[UpOffset + UpK - 1] = UpperA;

	for (int D = 0; D <= MaxD; D++) {

		// Extend the forward path.
		for (int k = DownK - D; k <= DownK + D; k += 2) {
			// Debug.Write(0, "SMS", "extend forward path " + k.ToString());

			// find the only or better starting point
			int x, y;
			if (k == DownK - D) {
				x = DownVector[DownOffset + k + 1]; // down
			} else {
				x = DownVector[DownOffset + k - 1] + 1; // a step to the right
				if ((k < DownK + D) && (DownVector[DownOffset + k + 1] >= x))
					x = DownVector[DownOffset + k + 1]; // down
			}
			y = x - k;

			// find the end of the furthest reaching forward D-path in diagonal k.
			while ((x < UpperA) && (y < UpperB) && (DataA.data[x] == DataB.data[y])) {
				x++; y++;
			}
			DownVector[DownOffset + k] = x;

			// overlap ?
			if (oddDelta && (UpK - D < k) && (k < UpK + D)) {
				if (UpVector[UpOffset + k] <= DownVector[DownOffset + k]) {
					ret.x = DownVector[DownOffset + k];
					ret.y = DownVector[DownOffset + k] - k;
					// ret.u = UpVector[UpOffset + k];      // 2002.09.20: no need for 2 points 
					// ret.v = UpVector[UpOffset + k] - k;
					return (ret);
				} // if
			} // if

		} // for k

		// Extend the reverse path.
		for (int k = UpK - D; k <= UpK + D; k += 2) {
			// Debug.Write(0, "SMS", "extend reverse path " + k.ToString());

			// find the only or better starting point
			int x, y;
			if (k == UpK + D) {
				x = UpVector[UpOffset + k - 1]; // up
			} else {
				x = UpVector[UpOffset + k + 1] - 1; // left
				if ((k > UpK - D) && (UpVector[UpOffset + k - 1] < x))
					x = UpVector[UpOffset + k - 1]; // up
			} // if
			y = x - k;

			while ((x > LowerA) && (y > LowerB) && (DataA.data[x - 1] == DataB.data[y - 1])) {
				x--; y--; // diagonal
			}
			UpVector[UpOffset + k] = x;

			// overlap ?
			if (!oddDelta && (DownK - D <= k) && (k <= DownK + D)) {
				if (UpVector[UpOffset + k] <= DownVector[DownOffset + k]) {
					ret.x = DownVector[DownOffset + k];
					ret.y = DownVector[DownOffset + k] - k;
					// ret.u = UpVector[UpOffset + k];     // 2002.09.20: no need for 2 points 
					// ret.v = UpVector[UpOffset + k] - k;
					return (ret);
				} // if
			} // if

		} // for k

	} // for D

	throw _MyException("the algorithm should never come here.");
}

void StringDiff::LCS(DiffData& DataA, int LowerA, int UpperA, DiffData& DataB, int LowerB, int UpperB, 
					 QVector<int>& DownVector, QVector<int>& UpVector) 
{
	// Debug.Write(2, "LCS", String.Format("Analyse the box: A[{0}-{1}] to B[{2}-{3}]", LowerA, UpperA, LowerB, UpperB));

	// Fast walkthrough equal lines at the start
	while (LowerA < UpperA && LowerB < UpperB && DataA.data[LowerA] == DataB.data[LowerB]) {
		LowerA++; LowerB++;
	}

	// Fast walkthrough equal lines at the end
	while (LowerA < UpperA && LowerB < UpperB && DataA.data[UpperA - 1] == DataB.data[UpperB - 1]) {
		--UpperA; --UpperB;
	}

	if (LowerA == UpperA) {
		// mark as inserted lines.
		while (LowerB < UpperB)
			DataB.modified[LowerB++] = true;

	} else if (LowerB == UpperB) {
		// mark as deleted lines.
		while (LowerA < UpperA)
			DataA.modified[LowerA++] = true;

	} else {
		// Find the middle snakea and length of an optimal path for A and B
		SMSRD smsrd = SMS(DataA, LowerA, UpperA, DataB, LowerB, UpperB, DownVector, UpVector);
		// Debug.Write(2, "MiddleSnakeData", String.Format("{0},{1}", smsrd.x, smsrd.y));

		// The path is from LowerX to (x,y) and (x,y) to UpperX
		LCS(DataA, LowerA, smsrd.x, DataB, LowerB, smsrd.y, DownVector, UpVector);
		LCS(DataA, smsrd.x, UpperA, DataB, smsrd.y, UpperB, DownVector, UpVector);  // 2002.09.20: no need for 2 points 
	}
}

QVector<StringDiff::Item> StringDiff::CreateDiffs(const DiffData& DataA, const DiffData& DataB)
{
	QList<Item> a;

	int StartA, StartB;
	int LineA, LineB;

	LineA = 0;
	LineB = 0;
	while (LineA < DataA.Length || LineB < DataB.Length) {
		if ((LineA < DataA.Length) && (!DataA.modified[LineA])
			&& (LineB < DataB.Length) && (!DataB.modified[LineB])) {
				// equal lines
				LineA++;
				LineB++;

		} else {
			// maybe deleted and/or inserted lines
			StartA = LineA;
			StartB = LineB;

			while (LineA < DataA.Length && (LineB >= DataB.Length || DataA.modified[LineA]))
				// while (LineA < DataA.Length && DataA.modified[LineA])
				LineA++;

			while (LineB < DataB.Length && (LineA >= DataA.Length || DataB.modified[LineB]))
				// while (LineB < DataB.Length && DataB.modified[LineB])
				LineB++;

			if ((StartA < LineA) || (StartB < LineB)) {
				// store a new difference-item
				Item aItem;
				aItem.StartA = StartA;
				aItem.StartB = StartB;
				aItem.deletedA = LineA - StartA;
				aItem.insertedB = LineB - StartB;
				a.append(aItem);
			} // if
		} // if
	} // while

	return a.toVector();
}


/// <summary>
/// This Class implements the Difference Algorithm published in
/// "An O(ND) Difference Algorithm and its Variations" by Eugene Myers
/// Algorithmica Vol. 1 No. 2, 1986, p 251.  
/// 
/// There are many C, Java, Lisp implementations public available but they all seem to come
/// from the same source (diffutils) that is under the (unfree) GNU public License
/// and cannot be reused as a sourcecode for a commercial application.
/// There are very old C implementations that use other (worse) algorithms.
/// Microsoft also published sourcecode of a diff-tool (windiff) that uses some tree data.
/// Also, a direct transfer from a C source to C# is not easy because there is a lot of pointer
/// arithmetic in the typical C solutions and i need a managed solution.
/// These are the reasons why I implemented the original published algorithm from the scratch and
/// make it avaliable without the GNU license limitations.
/// I do not need a high performance diff tool because it is used only sometimes.
/// I will do some performace tweaking when needed.
/// 
/// The algorithm itself is comparing 2 arrays of numbers so when comparing 2 text documents
/// each line is converted into a (hash) number. See DiffText(). 
/// 
/// Some chages to the original algorithm:
/// The original algorithm was described using a recursive approach and comparing zero indexed arrays.
/// Extracting sub-arrays and rejoining them is very performance and memory intensive so the same
/// (readonly) data arrays are passed arround together with their lower and upper bounds.
/// This circumstance makes the LCS and SMS functions more complicate.
/// I added some code to the LCS function to get a fast response on sub-arrays that are identical,
/// completely deleted or inserted.
/// 
/// The result from a comparisation is stored in 2 arrays that flag for modified (deleted or inserted)
/// lines in the 2 data arrays. These bits are then analysed to produce a array of Item objects.
/// 
/// Further possible optimizations:
/// (first rule: don't do it; second: don't do it yet)
/// The arrays DataA and DataB are passed as parameters, but are never changed after the creation
/// so they can be members of the class to avoid the paramter overhead.
/// In SMS is a lot of boundary arithmetic in the for-D and for-k loops that can be done by increment
/// and decrement of local variables.
/// The DownVector and UpVector arrays are alywas created and destroyed each time the SMS gets called.
/// It is possible to reuse tehm when transfering them to members of the class.
/// See TODO: hints.
/// 
/// diff.cs: A port of the algorythm to C#
/// Copyright (c) by Matthias Hertel, http://www.mathertel.de
/// This work is licensed under a BSD style license. See http://www.mathertel.de/License.aspx
/// 
/// Changes:
/// 2002.09.20 There was a "hang" in some situations.
/// Now I undestand a little bit more of the SMS algorithm. 
/// There have been overlapping boxes; that where analyzed partial differently.
/// One return-point is enough.
/// A assertion was added in CreateDiffs when in debug-mode, that counts the number of equal (no modified) lines in both arrays.
/// They must be identical.
/// 
/// 2003.02.07 Out of bounds error in the Up/Down vector arrays in some situations.
/// The two vetors are now accessed using different offsets that are adjusted using the start k-Line. 
/// A test case is added. 
/// 
/// 2006.03.05 Some documentation and a direct Diff entry point.
/// 
/// 2006.03.08 Refactored the API to static methods on the Diff class to make usage simpler.
/// 2006.03.10 using the standard Debug class for self-test now.
///            compile with: csc /target:exe /out:diffTest.exe /d:DEBUG /d:TRACE /d:SELFTEST Diff.cs
/// 2007.01.06 license agreement changed to a BSD style license.
/// 2007.06.03 added the Optimize method.
/// 2007.09.23 UpVector and DownVector optimization by Jan Stoklasa ().
/// 2008.05.31 Adjusted the testing code that failed because of the Optimize method (not a bug in the diff algorithm).
/// 2008.10.08 Fixing a test case and adding a new test case.
/// </summary>
