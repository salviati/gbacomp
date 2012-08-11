/*
   Copyright (c) Utkan Güngördü <utkan@freeconsole.org>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 3 or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of

   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the

   GNU General Public License for more details


   You should have received a copy of the GNU General Public
   License along with this program; if not, write to the
   Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

package gbacomp

import (
	"io/ioutil"
	"os"
	"testing"
)

var (
	testdata  [][]byte
	testfiles = []string{"testdata/Mark.Twain-Tom.Sawyer.txt", "testdata/e.txt", "testdata/pi.txt"}
	methods   = []Method{RLE, LZ77, Huffman4, Huffman8}
)

func load(path string) []byte {
	f, err := os.Open(path)
	if err != nil {
		panic(err)
	}
	data, err := ioutil.ReadAll(f)
	if err != nil {
		panic(err)
	}
	return data
}

func init() {

	testdata = make([][]byte, len(testfiles))
	for i, path := range testfiles {
		testdata[i] = load(path)
	}
}

func cmp(orig, prod []byte) (ok bool, n int) {
	if len(orig) > len(prod) {
		return false, -1
	}

	for i := 0; i < len(orig); i++ {
		if orig[i] != prod[i] {
			return false, i
		}
	}
	return true, -1
}

func TestRLE(t *testing.T) {
	for _, method := range methods {
		for datai, data := range testdata {
			t.Log("Testting", method, "with", testfiles[datai])
			c, err := Compress(method, data)
			if err != nil {
				t.Error("Compress:", err)
			}

			d, err := Decompress(method, c)

			if err != nil {
				t.Error("Decompress:", err)
			}

			if ok, n := cmp(data, d); !ok {
				t.Error("Decompressed data is not the same as the original one. Original length:", len(data), "After compression & decompression:", len(d), "Differs at position:", n)
				//t.Error("Output:", d)
			}
		}
	}
}
