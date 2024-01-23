# Copyright 2013-2023 Lawrence Livermore National Security, LLC and other
# Spack Project Developers. See the top-level COPYRIGHT file for details.
#
# SPDX-License-Identifier: (Apache-2.0 OR MIT)

from spack.package import *
from spack.pkg.builtin.openmpi import Openmpi as BuiltinOpenmpi 

class Openmpi(BuiltinOpenmpi):

    git = "https://github.com/tsukuba-hpcs/ompi-pmembb.git"

    version("5.0.0rc12-pmembb", branch="pmembb", submodules=True)
    version("5.0.0rc12-pmembb-eval", branch="pmembb-eval", submodules=True)

    variant('pmembb', default=False, description='Support pmembb', when='+romio')
    variant('aggregate_read', default=True, description='Enable aggregate read optimization', when='+pmembb')
    variant('debug', default=False, description='Enable debug')

    def configure_args(self):
        spec = self.spec
        config_args = super(Openmpi, self).configure_args()
        if spec.satisfies("@5:") and not spec.satisfies("schedulers=tm"):
            config_args.append("--without-pbs")
        if spec.satisfies("+pmembb"):
            romio_flags=[
                "--with-file-system=testfs+ufs+pmembb",
            ]
            if spec.satisfies("+aggregate_read"):
                romio_flags.append("--enable-pmembb-aggregate-read")
            else:
                romio_flags.append("--disable-pmembb-aggregate-read")

            config_args.append("--with-io-romio-flags=" + " ".join(romio_flags))
        if spec.satisfies("+debug"):
            config_args.append("--enable-debug")

        return config_args
