version=$1
dir=$2

./scripts/download_rpm_package.sh gcc $version $dir
./scripts/download_rpm_package.sh glibc $version $dir
./scripts/download_rpm_package.sh glibc-devel $version $dir
./scripts/download_rpm_package.sh libgcc $version $dir
./scripts/download_rpm_package.sh libstdc++ $version $dir
./scripts/download_rpm_package.sh libstdc++-devel $version $dir
./scripts/download_rpm_package.sh libstdc++-static $version $dir

./scripts/download_rpm_package.sh boost-devel $version $dir
./scripts/download_rpm_package.sh boost-static $version $dir
./scripts/download_rpm_package.sh boost-system $version $dir
