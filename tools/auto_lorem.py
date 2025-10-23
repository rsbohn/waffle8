#!/usr/bin/env python3
"""
Auto Manufacturer Lorem Ipsum Generator

Generates lorem ipsum-style text using automotive terminology,
car manufacturers, models, and industry jargon.
"""

import random

# Auto manufacturer names
MANUFACTURERS = [
    "Ford", "Chevrolet", "Toyota", "Honda", "Nissan", "BMW", "Mercedes",
    "Volkswagen", "Audi", "Porsche", "Ferrari", "Lamborghini", "Jaguar",
    "Volvo", "Subaru", "Mazda", "Hyundai", "Kia", "Lexus", "Acura",
    "Cadillac", "Buick", "Lincoln", "Infiniti", "Genesis", "Alfa Romeo",
    "Maserati", "Bentley", "Rolls-Royce", "McLaren", "Aston Martin"
]

# Car models and types
MODELS = [
    "Sedan", "Coupe", "Hatchback", "SUV", "Crossover", "Wagon", "Convertible",
    "Pickup", "Roadster", "Minivan", "Camaro", "Mustang", "Corvette", "911",
    "Civic", "Accord", "Prius", "Model S", "F-150", "Silverado", "Tacoma",
    "Wrangler", "Cherokee", "Pilot", "CR-V", "RAV4", "Highlander", "Explorer"
]

# Automotive terminology
AUTO_TERMS = [
    "engine", "transmission", "drivetrain", "chassis", "suspension", "brakes",
    "steering", "exhaust", "intake", "turbo", "supercharger", "hybrid",
    "electric", "horsepower", "torque", "displacement", "cylinders",
    "fuel injection", "carburetor", "differential", "clutch", "gearbox",
    "radiator", "cooling", "lubrication", "timing", "camshaft", "crankshaft",
    "piston", "valve", "spark plug", "alternator", "battery", "starter"
]

# Action words and descriptors
DESCRIPTORS = [
    "powerful", "efficient", "reliable", "innovative", "sleek", "aerodynamic",
    "robust", "advanced", "premium", "luxury", "sporty", "rugged", "smooth",
    "responsive", "precise", "durable", "lightweight", "high-performance",
    "fuel-efficient", "eco-friendly", "cutting-edge", "state-of-the-art"
]

ACTIONS = [
    "accelerates", "handles", "performs", "delivers", "provides", "features",
    "incorporates", "utilizes", "employs", "integrates", "enhances", "optimizes",
    "maximizes", "achieves", "maintains", "operates", "functions", "responds"
]

def generate_auto_sentence(min_words=8, max_words=20):
    """Generate a single automotive-themed sentence."""
    length = random.randint(min_words, max_words)
    words = []
    
    # Start with a manufacturer or model
    if random.choice([True, False]):
        words.append(random.choice(MANUFACTURERS))
    else:
        words.append("The")
        words.append(random.choice(MODELS))
    
    # Add an action verb
    words.append(random.choice(ACTIONS))
    
    # Fill with descriptors and terms
    while len(words) < length - 2:
        if random.random() < 0.4:  # 40% chance for descriptor
            words.append(random.choice(DESCRIPTORS))
        else:  # 60% chance for auto term
            words.append(random.choice(AUTO_TERMS))
    
    # End with a final term or descriptor
    words.append(random.choice(AUTO_TERMS + DESCRIPTORS))
    
    # Capitalize first word and add period
    sentence = " ".join(words)
    sentence = sentence[0].upper() + sentence[1:] + "."
    
    return sentence

def generate_auto_paragraph(sentences=5):
    """Generate a paragraph of automotive lorem ipsum."""
    return " ".join(generate_auto_sentence() for _ in range(sentences))

def generate_auto_lorem(paragraphs=3, sentences_per_paragraph=5):
    """Generate multiple paragraphs of automotive lorem ipsum."""
    return "\n\n".join(
        generate_auto_paragraph(sentences_per_paragraph) 
        for _ in range(paragraphs)
    )

if __name__ == "__main__":
    # Generate sample automotive lorem ipsum
    print("Auto Manufacturer Lorem Ipsum")
    print("=" * 50)
    print()
    
    # Generate 3 paragraphs
    lorem_text = generate_auto_lorem(3, 4)
    print(lorem_text)
    print()
    
    # Generate a shorter version for tape
    print("Short version for tape:")
    print("-" * 30)
    short_text = generate_auto_lorem(2, 3)
    print(short_text)