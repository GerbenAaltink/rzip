# Rzip

## Table of Contents
1. [Introduction](#introduction)
2. [Build Process](#build-process)
4. [Benchmarking](#benchmarking)
5. [RZIP Compression](#rzip-compression)
6. [Code Coverage](#code-coverage)

## Introduction

This project provides a comprehensive set of tools for managing backups, restoring data, compressing files, benchmarking performance, and analyzing code coverage. It uses a Makefile-based
 build system to automate these processes.

## Build Process

To run the build process, execute the following command in the terminal:

```bash
make all
``

This will execute all defined targets in the Makefile, including backup, backup_restore, precompressed, build, benchmark, rzip, and coverage.

## Backup and Restore

The backup target creates backups of critical data:

```bash
make backup
``

The backup_restore target restores previously created backups:

```bash
make backup_restore
``

## Precompression

The precompressed target prepares files for compression before building:

```bash
make precompressed
``

## Benchmarking

The benchmark target runs performance tests on the built software:

```bash
make benchmark
``

## RZIP Compression

The rzip target compresses fil using the RZIP algorithm:

```bash
make rzip
``

## Code Coverage

The coverage target generates reports on code coverage metrics:

```bash
make coverage
``

## License

This project is licensed under the MIT License - see the [LICENSE.md](LICENSE.md) file for details.


