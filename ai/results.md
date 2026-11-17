# Training Results Log

## Run 1: YOLO11n (2026-10-04)

**Settings:** `yolo11n.pt` (COCO-pretrained), 100 epochs, imgsz 640, batch 16, Google Colab T4
**Dataset:** Roboflow project-raggj/strawberry-diseases-detection-abknq (latest version), 9 classes
**Weights:** `MyDrive/hydro_tower/runs/yolo11n/weights/best.pt`

### Test split
| Metric | Value |
|---|---|
| mAP50 | **0.881** (dataset author's YOLO11n baseline: 0.839) |
| mAP50-95 | 0.683 |

| Class | mAP50-95 |
|---|---|
| Leaf Spot | 0.901 |
| Blossom Blight | 0.849 |
| Powdery Mildew Leaf | 0.786 |
| Angular Leafspot | 0.785 |
| Powdery Mildew Fruit | 0.764 |
| Anthracnose Fruit Rot | 0.649 |
| Gray Mold | 0.538 |
| Healthy Fruit | 0.480 |
| Healthy Leaf | 0.393 |

### Observations
- Train and validation losses both decrease and plateau, so there's no sign of overfitting. The mAP plateau starts around epoch 80. The loss drop near epoch 90 is the `close_mosaic` phase (mosaic augmentation disabled for the last 10 epochs).
- Fruit diseases are confused with each other: Anthracnose → Gray Mold 24%, Powdery Mildew Fruit → Gray Mold 24%.
- False alarms: Healthy Leaf → Powdery Mildew Leaf 23%. Detections on unlabeled background: Powdery Mildew Leaf 30%, Leaf Spot 25%.
- Mitigation in the Jetson logic: a confidence threshold of about 0.6–0.7, persistence across consecutive checks, and alert-only for disease classes.

### Jetson deployment (2026-10-10)
**Hardware:** Jetson Orin Nano 8 GB, JetPack 6.2 (L4T R36.4.7), USB webcam
**Software:** torch 2.10.0 (Jetson wheel), ultralytics 8.4.x, TensorRT 10.3.0
**Engine:** `best.engine`, TensorRT FP16, imgsz 640, workspace 2 GB, built on-device in 431.8 s

| Test | Result |
|---|---|
| Live webcam, phone screen showing Wikimedia Commons Leaf Spot photos | Detected "Leaf Spot" in 21/21 frames |
| Inference time (21 frames) | min 5.4 ms, max 18.8 ms, **mean ≈ 11.9 ms (≈ 84 FPS)** |

These are inference-only times with the display window and Firefox running, in the default power mode. Pre- and post-processing aren't included. The webcam caps end-to-end FPS at about 30.

### Next experiments
- YOLO11s and YOLOv8n with the same settings (accuracy vs Jetson FPS)
- External test on unseen internet photos, especially healthy plants, to measure false alarms
