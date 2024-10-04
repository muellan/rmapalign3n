# RMapAlign3N v0.1.0

RMapAlign3N is a tool that maps next generation sequencing reads to a reference genome or reference transcriptome.
In particular, it is aimed for processing reads stemming from 3N nucleotide conversion treatment like bisulfite sequencing where one nucleobase is exchanged for another based on the presence or absence of a modification in the original biomelecule like, e.g., methylation.




## Installation

#### Requirements
RMapAlign3N itself should compile on any platform for which a C++17 conforming compiler is available.
Dependencies for the default mode are included in the repository as source.
For BAM output htslib is required, which is included only for linux x86-64 as a static library.


#### Compile
Run 'make' in the directory containing the Makefile.
This will compile RMapAlign3N with the default data type settings.


##### BAM support
To compile RMapAlign3N with support for the BAM output format, htslib is required, which is only included as a static library for linux (x86-64), and must otherwise be installed manually and the Makefile adjusted accordingly.

* To compile with BAM support, start make with the RMA_BAM=TRUE environment variable:
  ```
  RMA_BAM=TRUE make
  ```

In rare cases databases built on one platform might not work with RMapAlign3N on other platforms due to bit-endianness and data type width differences. Especially mixing RMapAlign3N executables compiled with 32-bit and 64-bit compilers might be probelematic.


##### different kmer lengths
You can compile RMapAlign3N with support for greater k-mer lengths.

* support for kmer lengths up to 16 (default):
  ```
  make MACROS="-DMC_KMER_TYPE=uint32_t"
  ```

* support for kmer lengths up to 32 (needs more memory):
  ```
  make MACROS="-DMC_KMER_TYPE=uint64_t"
  ```

Note that a database can only be queried with the same variant of RMapAlign3N (regarding data type sizes) that it was built with.

In rare cases databases built on one platform might not work with RMapAlign3N on other platforms due to bit-endianness and data type width differences. Especially mixing RMapAlign3N executables compiled with 32-bit and 64-bit compilers might be probelematic.




## Building a Reference Database

RmapAlign3N's [build mode](docs/mode_build.txt) is used for creating databases.
Reference Files must be in FASTA or FASTQ format.

You can either specify all input files separately:
```
rmapalign3n build myrefdb -conv C T reference1.fq reference2.fq reference3.fq
```

or put them all in one folder and then use that folder as input source:
```
rmapalign3n build myrefdb -conv C T reference_folder
```

The option `-conv <X> <Y>` specifies which nucleotide conversion should be applied, e.g., for BS-seq one would use
the default `-conv C T` for a C to T conversion.




## Mapping / Aligning
Once a database is built you can map reads.
* a single FASTQ file containing some reads:
  ```
  ./rmapalign3n query myrefdb my_reads.fq -out results.txt
  ```
* with SAM output:
  ```
  ./rmapalign3n query myrefdb my_reads.fq -sam -out results.sam
  ```
* with SAM + alignment output:
  ```
  ./rmapalign3n query myrefdb my_reads.fq -sam -align -out results.sam
  ```
* an entire directory containing FASTA/FASTQ files:
  ```
  ./rmapalign3n query myrefdb my_folder -out ...
  ```
* paired-end reads in separate files:
  ```
  ./rmapalign3n query myrefdb my_reads1.fq my_reads2.fq -pairfiles -out ...
  ```
* paired-end reads in one file (a1,a2,b1,b2,...):
  ```
  ./rmapalign3n query myrefdb my_paired_reads.fq -pairseq -out ...
  ```



## Documentation of Command Line Parameters

* [for mode `build`](docs/mode_build.txt): build database from reference sequences
* [for mode `query`](docs/mode_query.txt): query reads against database


View options documentation from the command line with
```
./rmapalign3n help build
./rmapalign3n help query
```

