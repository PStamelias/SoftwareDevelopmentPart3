#!/bin/bash
until ./testdriver;[ $? -eq 139 ]; do printf '.'; done