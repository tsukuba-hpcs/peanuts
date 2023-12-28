# Copyright 2013-2023 Lawrence Livermore National Security, LLC and other
# Spack Project Developers. See the top-level COPYRIGHT file for details.
#
# SPDX-License-Identifier: (Apache-2.0 OR MIT)

from spack.package import *


class Rpmembb(CMakePackage):
    """rpmembb: aggregate node-local persistent memory by MPI RMA"""

    homepage = "https://github.com/tsukuba-hpcs/rpmembb"
    # url = ""
    git      = "https://github.com/tsukuba-hpcs/rpmembb.git"


    maintainers("range3")

    version("master", branch="master")
    version("0.1.0", tag="v0.1.0", submodules=True)

    depends_on("mpi")

    conflicts("%gcc@:9")

    def setup_build_environment(self, env):
        env.unset('CPM_SOURCE_CACHE')

    def cmake_args(self):
        args = []
        return args
