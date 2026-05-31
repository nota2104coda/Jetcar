# SOTIF & Functional Safety Analysis: Obstacle Avoidance

This document applies ISO 21448 (SOTIF) and ISO 26262 principles to the Jetcar's obstacle avoidance system.

## 1. System Definition
The "Obstacle Avoidance" feature uses a combination of:
- **Sensors**: LD06 Lidar, Realsense D435 (Vision), LD2450 (Radar).
- **Processing**: YOLOv8 (Object Detection), Nav2 (Local Planner).
- **Actuation**: 4-Motor Tank Drive.

## 2. Hazard Analysis (ISO 26262 / SOTIF)

| Hazard ID | Hazardous Event | Triggering Condition (SOTIF) | Potential Effect |
|-----------|-----------------|------------------------------|------------------|
| H-01 | Collision with static object | Lidar "transparent" surface (glass) | Property Damage |
| H-02 | Collision with human | Radar interference or blind spot | Personal Injury |
| H-03 | Failure to stop for obstacle | High latency in YOLO processing | High-speed impact |
| H-04 | "Phantom" braking | Dust/Steam mistaken for obstacle | Abrupt stop (system instability) |

## 3. SOTIF Triggering Conditions
*   **Sensor Insufficiency**: Realsense camera blinded by direct sunlight (glare).
*   **Algorithm Limitation**: YOLO failing to detect a "blue ball" if it hasn't been trained on that specific hue/size.
*   **Environment**: Transition from bright light to dark corridor causing auto-exposure lag.

## 4. Safety Requirements (SR)
- **SR-01**: The system shall detect obstacles > 10cm within 2 meters.
- **SR-02**: The system shall initiate braking within 100ms of a confirmed collision trajectory.
- **SR-03**: The Raspberry Pi Pico "Safety Layer" shall override Jetson commands if cliff sensors or sonar detect immediate danger.

## 5. Test Case Mapping (Project 1)

| Req ID | Test Case ID | Simulation Scenario | Success Criteria |
|--------|--------------|---------------------|------------------|
| SR-01 | TC-01-STATIC | Spawn a box in path | Robot replans or stops |
| SR-02 | TC-02-DYNAMIC | Roll a ball across path | Robot stops before impact |
| SR-03 | TC-03-FAULT | Inject "No Lidar" fault | Robot enters Safe State (Stop) |
