package main

import (
	"github.com/salviati/gbacomp"
	"flag"
	"log"
	"os"
	"io/ioutil"
)

var (
	//method = flag.String("method", "", "Compression method: rle,lz77,huff8,huff4. When not set, does decompression instead.")
	method = flag.String("method", "", "Compression method: rle,lz77,huff8. When not set, does decompression instead.")
	inname = flag.String("i", "", "input filename")
	outname = flag.String("o", "", "output filename")
	
	gbacompMethod = map[string]gbacomp.Method{"lz77": gbacomp.LZ77, "rle": gbacomp.RLE, /*"huff4":gbacomp.Huffman4,*/ "huff8":gbacomp.Huffman8 }
)

func chk(err error) {
	if err != nil {
		log.Fatal(err)
	}
}

func main() {
	flag.Parse()
	
	in , err := os.Open(*inname)
	chk(err)

	out, err := os.OpenFile(*outname, os.O_WRONLY | os.O_CREATE, 0666)
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
	
	out.Write(outData)
}