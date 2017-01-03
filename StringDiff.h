#ifndef STRINGDIFF_H
#define STRINGDIFF_H

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
#include <QString>
#include <QVector>
#include <QHash>

namespace Ds
{
	class StringDiff 
	{
	public:
		/// <summary>details of one difference.</summary>
		struct Item
		{
		  /// <summary>Start Line number in Data A.</summary>
		  int StartA;
		  /// <summary>Start Line number in Data B.</summary>
		  int StartB;

		  /// <summary>Number of changes in Data A.</summary>
		  int deletedA;
		  /// <summary>Number of changes in Data B.</summary>
		  int insertedB;
		  Item():StartA(0),StartB(0),deletedA(0),insertedB(0){}
		}; 
		typedef QVector<Item> Items;



		/// <summary>
		/// Find the difference in 2 arrays of integers.
		/// </summary>
		/// <param name="ArrayA">A-version of the numbers (usualy the old one)</param>
		/// <param name="ArrayB">B-version of the numbers (usualy the new one)</param>
		/// <returns>Returns a array of Items that describe the differences.</returns>
		static QVector<Item> Diff(const QString& ArrayA, const QString& ArrayB);
	private:
		/// <summary>
		/// Shortest Middle Snake Return Data
		/// </summary>
		struct SMSRD
		{
		  int x, y;
		  SMSRD():x(0),y(0){}
		};

		/// <summary>Data on one input file being compared.  
		/// </summary>
		struct DiffData
		{

			/// <summary>Number of elements (lines).</summary>
			int Length;

			/// <summary>Buffer of numbers that will be compared.</summary>
			QString data;

			/// <summary>
			/// Array of booleans that flag for modified data.
			/// This is the result of the diff.
			/// This means deletedA in the first Data or inserted in the second Data.
			/// </summary>
			QVector<bool> modified;

			/// <summary>
			/// Initialize the Diff-Data buffer.
			/// </summary>
			/// <param name="data">reference to the buffer</param>
			DiffData(const QString& initData) {
			  data = initData;
			  Length = initData.size();
			  modified.resize( Length + 2 );
			} 
			DiffData():Length(0){}
		}; 

		/// <summary>
		/// This is the algorithm to find the Shortest Middle Snake (SMS).
		/// </summary>
		/// <param name="DataA">sequence A</param>
		/// <param name="LowerA">lower bound of the actual range in DataA</param>
		/// <param name="UpperA">upper bound of the actual range in DataA (exclusive)</param>
		/// <param name="DataB">sequence B</param>
		/// <param name="LowerB">lower bound of the actual range in DataB</param>
		/// <param name="UpperB">upper bound of the actual range in DataB (exclusive)</param>
		/// <param name="DownVector">a vector for the (0,0) to (x,y) search. Passed as a parameter for speed reasons.</param>
		/// <param name="UpVector">a vector for the (u,v) to (N,M) search. Passed as a parameter for speed reasons.</param>
		/// <returns>a MiddleSnakeData record containing x,y and u,v</returns>
		static SMSRD SMS(DiffData& DataA, int LowerA, int UpperA, DiffData& DataB, int LowerB, int UpperB,
			QVector<int>& DownVector, QVector<int>& UpVector);

		/// <summary>
		/// This is the divide-and-conquer implementation of the longes common-subsequence (LCS) 
		/// algorithm.
		/// The published algorithm passes recursively parts of the A and B sequences.
		/// To avoid copying these arrays the lower and upper bounds are passed while the sequences stay constant.
		/// </summary>
		/// <param name="DataA">sequence A</param>
		/// <param name="LowerA">lower bound of the actual range in DataA</param>
		/// <param name="UpperA">upper bound of the actual range in DataA (exclusive)</param>
		/// <param name="DataB">sequence B</param>
		/// <param name="LowerB">lower bound of the actual range in DataB</param>
		/// <param name="UpperB">upper bound of the actual range in DataB (exclusive)</param>
		/// <param name="DownVector">a vector for the (0,0) to (x,y) search. Passed as a parameter for speed reasons.</param>
		/// <param name="UpVector">a vector for the (u,v) to (N,M) search. Passed as a parameter for speed reasons.</param>
		static void LCS(DiffData& DataA, int LowerA, int UpperA, DiffData& DataB, int LowerB, int UpperB, QVector<int>& DownVector, QVector<int>& UpVector);

		/// <summary>Scan the tables of which lines are inserted and deleted,
		/// producing an edit script in forward order.  
		/// </summary>
		/// dynamic array
		static QVector<Item> CreateDiffs(const DiffData& DataA, const DiffData& DataB);
	};
}
#endif // STRINGDIFF_H
