## Student
Ahmet Yaşa

## Description

This project implements a custom animation model plugin for N8RO.

A new animation model called `animationModelCustom` is registered through the plugin.

When the animation state is set to `Custom Idle`, the model overrides arm joints and generates a simple idle motion using sinusoidal movement.

## Motion State

Animation State:
- Custom Idle

Affected Joints:
- leftShoulder
- rightShoulder
- leftElbow
- rightElbow

## Video Demonstration

https://www.youtube.com/watch?v=THx-WlCydoU

## Files

- SimCharAnimCustomModelPlugin.cpp
- SimCharAnimCustomModelPlugin.h
- AnimationModelCustom.json.gz
