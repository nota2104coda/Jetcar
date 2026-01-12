from nano_llm import NanoLLM
import logging

class VLMInferenceService:
    """Lazy-loaded VLM inference service"""
    
    def __init__(self, model='liuhaotian/llava-v1.5-7b'):
        logging.info(f"Loading VLM model: {model}")
        
        self.model = NanoLLM.from_pretrained(
            model,
            api="mlc",
            quantization="q4f16_ft",  # Lightweight quantization
            vision_api="hf"
        )
        
        # Warm up the model
        self._warmup()
        logging.info("VLM model loaded and warmed up")
    
    def _warmup(self):
        """Warm up model with dummy inference"""
        from PIL import Image
        import numpy as np
        
        dummy_image = Image.fromarray(
            np.zeros((224, 224, 3), dtype=np.uint8)
        )
        _ = self.model.generate(
            dummy_image,
            "What is this?",
            max_new_tokens=10
        )
    
    def infer(self, image, prompt):
        """Run VLM inference"""
        response = self.model.generate(
            image,
            prompt,
            max_new_tokens=128
        )
        return response