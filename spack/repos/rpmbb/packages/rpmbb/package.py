# Copyright 2013-2023 Lawrence Livermore National Security, LLC and other
# Spack Project Developers. See the top-level COPYRIGHT file for details.
#
# SPDX-License-Identifier: (Apache-2.0 OR MIT)

from spack.package import *


class Rpmbb(CMakePackage):
    """rpmbb: aggregate node-local persistent memory by MPI RMA"""

    homepage = "https://github.com/tsukuba-hpcs/rpmembb"
    # url = ""
    git      = "https://github.com/tsukuba-hpcs/rpmembb.git"

    maintainers("range3")

    variant("deferred_open", default=True, description="use deferred open")
    variant("agg_read", default=True, description="use aggregate read")

    version("master", branch="master")
    version("0.8.0", tag="v0.8.0")

    depends_on("mpi")
    depends_on("pmdk+ndctl")
    depends_on("liburing")
    depends_on("pkgconfig", type="build")

    conflicts("%gcc@:9")

    def setup_build_environment(self, env):
        env.unset("CPM_SOURCE_CACHE")

    def cmake_args(self):
        args = [
            self.define_from_variant("RPMBB_USE_DEFERRED_OPEN", "deferred_open"),
            self.define_from_variant("RPMBB_USE_AGG_READ", "agg_read"),
        ]
        return args
