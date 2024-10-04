/******************************************************************************
 *
 * RmapAlign3N - 3N Read Mapping and Alignment Tool
 *
 * Copyright (C) 2024 André Müller (muellan@uni-mainz.de)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *****************************************************************************/

#include <string>
#include <fstream>
#include <iostream>
#include <vector>

#include "options.h"
#include "filesys_utility.h"


namespace mc {


//-------------------------------------------------------------------
void main_mode_help(const cmdline_args& args)
{
    if (args.size() < 3 || args[1] != "help" || args[2] == "help") {

        if (args.size() > 1 && args[1] != "help") {
            std::cerr << "ERROR: Invalid command line arguments!\n\n";
        }
        else {
            std::cout <<
                "RmapAlign3N Copyright (C)  2024  André Müller\n"
                "This program comes with ABSOLUTELY NO WARRANTY.\n"
                "This is free software, and you are welcome to redistribute it\n"
                "under certain conditions. See the file 'LICENSE' for details.\n\n";
        }

        std::cout <<
            "USAGE:\n"
            "\n"
            "    rmapalign3n <MODE> [OPTION...]\n"
            "\n"
            "    Available modes:\n"
            "\n"
            "    build       build new database from reference sequence(s)\n"
            "    query       map reads using pre-built database\n"
            "    help        shows documentation \n"
            "\n"
            "\n"
            "EXAMPLES:\n"
            "\n"
            "    Map single FASTA file 'myreads.fna' against pre-built database 'refdb':\n"
            "        rmapalign3n query refdb myreads.fna -out results.txt\n"
            "    same with SAM output:\n"
            "        rmapalign3n query refdb myreads.fna -sam -out results.sam\n"
            "    same with SAM + alignment output:\n"
            "        rmapalign3n query refdb myreads.fna -sam -align -out results.sam\n"
            "\n"
            "    Map all sequence files in folder 'test' againgst 'refdb':\n"
            "        rmapalign3n query refdb test -out results.txt\n"
            "\n"
            "    Map paired-end reads in separate files against 'refdb':\n"
            "        rmapalign3n query refdb reads1.fa reads2.fa -pairfiles -out results.txt\n"
            "\n"
            "    Map paired-end reads in one file (a1,a2,b1,b2,...) against 'refdb':\n"
            "        rmapalign3n query refdb paired_reads.fa -pairseq -out results.txt\n"
            "    \n"
            "    View documentation for query mode:\n"
            "        rmapalign3n help query\n"
            "\n"
            "    View documentation on how to build databases:\n"
            "        rmapalign3n help build\n";
    }
    else if (args[2] == "build") {
        std::cout << build_mode_docs() << '\n';
    }
    else if (args[2] == "query") {
        std::cout << query_mode_docs() << '\n';
    }
    else if (args[2] == "info") {
        std::cout << info_mode_docs() << '\n';
    }
    else {
        std::cerr
            << "You need to specify a mode for which to show help :\n"
            << "    " << args[0] << " help <mode>\n\n"
            << "Unknown mode '" << args[2] << "'\n\n"
            << "Available modes are:\n"
            << "    build\n"
            << "    query\n";
    }
}


} // namespace mc
