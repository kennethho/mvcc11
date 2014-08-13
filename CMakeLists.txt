cmake_minimum_required(VERSION 2.8)

PROJECT(
        MVCC
        )

FIND_PACKAGE(
        Boost COMPONENTS unit_test_framework REQUIRED
        )

ADD_SUBDIRECTORY(
        test
        )
