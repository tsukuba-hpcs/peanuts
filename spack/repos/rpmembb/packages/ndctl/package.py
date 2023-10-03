from spack.package import *


class Ndctl(Package):
    """ndctl: A "device memory" enabling project encompassing tools and libraries
    for CXL, NVDIMMs, DAX, memory tiering and other platform memory device topics.
    """

    homepage = "https://github.com/pmem/ndctl"
    has_code = False

    maintainers("range3")

    version("78")

    # ndctl needs to be added as an external package to SPACK. For this, the
    # config file packages.yaml needs to be adjusted:
    #
    # packages:
    #   ndctl:
    #     buildable: False
    #     externals:
    #     - spec: ndctl@78
    #       prefix: /usr

    def install(self, spec, prefix):
        raise InstallError(
            self.spec.format(
                "{name} is not installable, you need to specify "
                "it as an external package in packages.yaml"
            )
        )
