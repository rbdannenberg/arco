from absl import app, logging
import torch, torchaudio
import os
import cached_conv as cc



torch.set_grad_enabled(False)

def load_model(model_path, device):
    """Load and prepare the RAVE model."""
    stream = True
    torch.set_float32_matmul_precision('high')
    cc.use_cached_conv(stream)
    cc.MAX_BATCH_SIZE = 8

    print(f"Using device: {device}")
    
    # Load from TorchScript
    if os.path.splitext(model_path)[1] == ".ts":
        model = torch.jit.load(model_path)
        if not hasattr(model, 'n_channels'): 
            print('Warning: model does not have n_channels attribute; setting to 1')
            model.n_channels = 1
        if not hasattr(model, 'sr'):
            print('Warning: model does not have sr attribute; setting to 44100')
            model.sr = 44100

    # Load from checkpoint: to be implemented
    else:
        raise Exception("Loading from non-torchscript files not currently supported.")
    
    return model.to(device)


def process_audio(model, audio_tensor : torch.Tensor, sample_rate, device):
    """Process audio tensor through the RAVE model.
    Args:
        model: RAVE model instance
        audio_tensor: torch.Tensor of shape (channels, samples)
        sample_rate: input sample rate
        device: torch device
    
    Returns:
        torch.Tensor: Processed audio tensor
    """
    

    if audio_tensor.ndim == 1:
        audio_tensor = audio_tensor.unsqueeze(0)
    elif audio_tensor.ndim > 2:
        raise ValueError(f"Expected 1D or 2D tensor, got shape {audio_tensor.shape}")

    if sample_rate != model.sr:
        audio_tensor = torchaudio.functional.resample(audio_tensor, sample_rate, model.sr)
    
    if model.n_channels != audio_tensor.shape[0]:
        
        if model.n_channels < audio_tensor.shape[0]:
            print("Warning: Model's n_channels < input number of channels, truncating input")
            audio_tensor = audio_tensor[:model.n_channels]
        else:
            raise ValueError(f'Input has {audio_tensor.shape[0]} channels, but model expects {model.n_channels}')
    
    audio_tensor = audio_tensor.to(device)

    with torch.device(device):
        # Add batch dimension => (1, channels, samples)
        audio_tensor = audio_tensor.unsqueeze(0)
        z = model.encode(audio_tensor)
        # Add latent manipulations? 
        output = model.decode(z)
    
    return output[0]


##########################File I/O utilities for testing only##########################

def load_audio_file(file_path):
    audio_tensor, sr = torchaudio.load(file_path)
    return audio_tensor, sr

def save_audio_file(tensor, sample_rate, file_path):
    os.makedirs(os.path.dirname(file_path), exist_ok=True)
    torchaudio.save(file_path, tensor.detach().cpu(), sample_rate)

def main(argv):
    if len(argv) != 4:
        print("Usage: script.py <model_path> <out_path> <input_file>")
        return 1
    
    model_path = argv[1]
    out_path = argv[2]
    input_file = argv[3]
    
    device = torch.device('cpu')
    print(f"Using device: {device}")
    
    # Load model with selected device
    model = load_model(model_path, device)
    
    # Process input file
    try:
        audio_tensor, sr = load_audio_file(input_file)
        print(f"Loaded audio: shape={audio_tensor.shape}, sr={sr}")
        
        output = process_audio(model, audio_tensor, sr, device)
        
        # Create output filename
        name = "_".join(os.path.basename(model_path).split('_')[:-1])
        output_path = os.path.join(out_path, name, os.path.basename(input_file))
        os.makedirs(os.path.dirname(output_path), exist_ok=True)
        
        save_audio_file(output, model.sr, output_path)
        print(f"Saved output to: {output_path}")
        
    except Exception as e:
        logging.error(f"Error processing file {input_file}: {str(e)}")
        return 1
    
    return 0

if __name__ == "__main__":
    app.run(main)