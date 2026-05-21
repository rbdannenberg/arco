## RAVE Python Server for ARCO

Real-time neural audio processing using RAVE (Realtime Audio Variational autoEncoder) models with ARCO's o2audioio system.

### Setup
1. Create Python 3.10+ environment
2. `pip install -r requirements.txt`
3. Download a RAVE model from https://acids-ircam.github.io/rave_models_download

**Note:** Currently configured for mono, 2048-sample model output chunks. Models with different output configurations (e.g. stereo) require modifications to `rave_o2.py` and `init.srp`.

**Tested models:** Darbouka_onnx, NASA 

### Usage
1. Start the RAVE server:
   ```bash
   python rave_o2.py --model=<your_model_path>.ts
   ```

2. Build and run ARCO test app, then enable "O2audioio Test" in the GUI



