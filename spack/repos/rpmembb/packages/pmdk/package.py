# Copyright 2013-2023 Lawrence Livermore National Security, LLC and other
# Spack Project Developers. See the top-level COPYRIGHT file for details.
#
# SPDX-License-Identifier: (Apache-2.0 OR MIT)

from spack.package import *
from spack.pkg.builtin.pmdk import Pmdk as BuiltinPmdk 

class Pmdk(BuiltinPmdk):
    version("2.0.0", sha256="85e3997e2a78057487b7b6db486283ae70f6ca4875254da2d38d45f847b63680")
    version("1.13.1", sha256="960a3d7ad83ff267e832586c34a88ced7915059a064a77e5984fcd4d4a235c6e")

    variant("rpmem", when="@:1.12", default=False, description="Build remote persistent memory components")

    depends_on("ndctl@63:", when="+ndctl")

    def install(self, spec, prefix):
        make_args = [
            "prefix=%s" % prefix,
            "EXTRA_CFLAGS=-Wno-error",
            "NDCTL_ENABLE={0}".format("y" if "+ndctl" in spec else "n"),
            "BUILD_RPMEM={0}".format("y" if "+rpmem" in spec else "n"),
            "DOC={0}".format("y" if "+doc" in spec else "n"),
            "EXPERIMENTAL={0}".format("y" if "+experimental" in spec else "n"),
        ]

        # prevent use of external pkg-config.
        if "+ndctl" in spec:
            make_args += [
                "OS_DIMM=ndctl",
                "LIBNDCTL_PKG_CONFIG_DEPS='libndctl libdaxctl'",
                "LIBNDCTL_PKG_CONFIG_DEPS_VAR=',libndctl,libdaxctl'",
                f"LIBNDCTL_CFLAGS=-I{spec['ndctl'].prefix.include}",
                f"LIBNDCTL_LD_LIBRARY_PATHS={spec['ndctl'].prefix.lib}",
                "LIBNDCTL_LIBS=-lndctl -ldaxctl",
                "OS_DIMM_CFLAG=-DNDCTL_ENABLED=1",
            ]

        # pmdk prior to 1.8 was particular about the ARCH specification, must
        # be exactly "x86_64" for build to work
        if spec.target.family == "x86_64":
            make_args += ["ARCH=x86_64"]

        make("install", *make_args)
