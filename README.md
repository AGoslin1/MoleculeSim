Overview
MoleculeSim is a molecular simulation and visualisation application that allows users to input a chemical 
formula and generate a 3D model of the molecule. The system uses external chemical databases and
processing tools to convert chemical representations into visual structures.

Features:
Input chemical formulas
Retrieve molecular data from PubChem
Generate 3D molecular structures using OpenBabel
Interactive 3D visualisation using 3Dmol.js
Isomer selection for compounds with multiple structures
Display of molecular properties and information
Tab system for viewing multiple molecules

Technologies Used
C++ – Core application
WebView2 – Embedded web interface
HTML/CSS/JavaScript – Front-end interface
3Dmol.js – 3D molecular rendering
PubChem API – Chemical data retrieval
OpenBabel – Molecular structure generation

Requirements

Before running the application, ensure the following are installed:

Windows OS
Microsoft WebView2 Runtime
OpenBabel (installed or available in system PATH)
Internet connection (required for PubChem API access)

How to Run
Clone or download the repository
Open the project in Visual Studio
Build the solution
Run the application
Enter a chemical formula in the input field
Select an isomer
View and interact with the 3D molecular model

OpenBabel download:
https://github.com/openbabel/openbabel/releases
download the OpenBabel-3.1.1.exe, run the wizard and download into C:\Program Files (not required but recommended)

Project Structure
MoleculeSim.cpp – Main application and WebView2 setup
Simulation. – Molecule data structures and logic
PubChemClient. – API requests and data parsing
RDKitBridge. – OpenBabel integration
viewer.html – Frontend interface

Author

Alex Goslin