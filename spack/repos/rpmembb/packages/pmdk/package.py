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
