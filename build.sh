#!/bin/bash

set -x

prepare_env()
{
    git submodule update --init
}

build_isa_sim()
{
    (
    cd riscv-isa-sim &&
    ./configure CPPFLAGS=-fPIC --enable-dirty &&
    make -j16
    )
}

build_qemu()
{
    cosim_lib=$PWD/riscv-isa-sim
    (
    name=build
    mkdir -p $name &&
    cd $name &&
    ../configure --target-list=riscv64-softmmu --lib-cosim=$cosim_lib &&
    make -j16
    )
}

prepare_env &&
build_isa_sim &&
build_qemu
