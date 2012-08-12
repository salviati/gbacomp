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

package main

import (
	"flag"
	"github.com/salviati/gbacomp"
	"io/ioutil"
	"log"
	"os"
)

var (
	method  = flag.String("method", "", "Compression method: rle,lz77,huff8,huff4. When not set, does decompression instead.")
	inname  = flag.String("i", "", "input filename")
	outname = flag.String("o", "", "output filename")

	gbacompMethod = map[string]gbacomp.Method{"lz77": gbacomp.LZ77, "rle": gbacomp.RLE, "huff4": gbacomp.Huffman4, "huff8": gbacomp.Huffman8}
)

func chk(err error) {
	if err != nil {
		log.Fatal(err)
	}
}

func main() {
	flag.Parse()

	if *inname == "" || *outname == "" {
		flag.PrintDefaults()
		return
	}

	in, err := os.Open(*inname)
	chk(err)

	out, err := os.OpenFile(*outname, os.O_WRONLY|os.O_CREATE, 0666)
	chk(err)

	inData, err := ioutil.ReadAll(in)
	chk(err)

	var outData []byte

	if *method == "" {
		outData, err = gbacomp.Decompress(inData)
		chk(err)
	} else {
		if m, ok := gbacompMethod[*method]; ok {
			outData, err = gbacomp.Compress(m, inData)
			chk(err)
		} else {
			log.Fatal("unknown method", *method)
		}
	}

	_, err = out.Write(outData)
	chk(err)
}
