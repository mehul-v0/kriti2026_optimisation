import { useState, useCallback } from 'react';
import { useNavigate } from 'react-router-dom';
import { motion } from 'framer-motion';
import { Upload, File, Check, X, Cloud, Folder, FileSpreadsheet } from 'lucide-react';
import { uploadFile } from '../services/api';
import { mapEmployees, mapVehicles } from '../utils/mappers';
import type { Employee, Vehicle } from '../types';

export default function DataUpload() {
  const navigate = useNavigate();
  const [uploadedFile, setUploadedFile] = useState<File | null>(null);
  const [parsedData, setParsedData] = useState<{ employees: Employee[]; vehicles: Vehicle[] } | null>(null);
  const [isValidating, setIsValidating] = useState(false);
  const [validationError, setValidationError] = useState<string | null>(null);
  const [dragActive, setDragActive] = useState(false);

  const handleDrag = useCallback((e: React.DragEvent) => {
    e.preventDefault();
    e.stopPropagation();
    if (e.type === 'dragenter' || e.type === 'dragover') {
      setDragActive(true);
    } else if (e.type === 'dragleave') {
      setDragActive(false);
    }
  }, []);

  const processFile = async (file: File) => {
    setIsValidating(true);
    setValidationError(null);

    try {
      // Upload to backend — backend converts Excel → JSON and returns parsed data
      const response = await uploadFile(file);

      if (!response.success) {
        throw new Error('Backend failed to process the file');
      }

      // Map backend format to frontend types
      const employees = mapEmployees(response.employees);
      const vehicles = mapVehicles(response.vehicles);

      if (employees.length === 0 || vehicles.length === 0) {
        throw new Error('Excel file must contain both Employees and Vehicles data');
      }

      setParsedData({ employees, vehicles });

      // Store backend raw data for the optimization step
      sessionStorage.setItem('uploadedData', JSON.stringify({
        employees,
        vehicles,
        // Keep raw backend data for the optimize call
        backendEmployees: response.employees,
        backendVehicles: response.vehicles,
        baselineCost: response.baseline_cost,
        filename: response.filename,
        digest: response.digest,
      }));

      setIsValidating(false);
    } catch (error: any) {
      setValidationError(error.message || 'Failed to upload and parse file');
      setParsedData(null);
      setIsValidating(false);
    }
  };

  const handleDrop = useCallback(async (e: React.DragEvent) => {
    e.preventDefault();
    e.stopPropagation();
    setDragActive(false);

    const files = e.dataTransfer.files;
    if (files && files[0]) {
      setUploadedFile(files[0]);
      await processFile(files[0]);
    }
  }, []);

  const handleFileSelect = async (e: React.ChangeEvent<HTMLInputElement>) => {
    const files = e.target.files;
    if (files && files[0]) {
      setUploadedFile(files[0]);
      await processFile(files[0]);
    }
  };

  const handleRemove = () => {
    setUploadedFile(null);
    setParsedData(null);
    setValidationError(null);
  };

  const handleContinue = () => {
    if (parsedData) {
      navigate('/insights');
    }
  };

  return (
    <div className="min-h-screen bg-dark p-8">
      <div className="max-w-4xl mx-auto">
        {/* Progress Indicator */}
        <div className="mb-8">
          <div className="flex items-center justify-between">
            {['Upload Data', 'Review Insights', 'Configure', 'Optimize'].map((step, index) => (
              <div key={step} className="flex items-center">
                <div
                  className={`w-10 h-10 rounded-full flex items-center justify-center font-bold ${
                    index === 0 ? 'bg-primary text-dark' : 'bg-dark-700 text-gray'
                  }`}
                >
                  {index + 1}
                </div>
                <span className={`ml-2 ${index === 0 ? 'text-white font-medium' : 'text-gray'}`}>
                  {step}
                </span>
                {index < 3 && <div className="w-12 h-px bg-gray/30 mx-4" />}
              </div>
            ))}
          </div>
        </div>

        <h1 className="text-4xl font-bold mb-2">Upload Your Data</h1>
        <p className="text-gray mb-8">Import employee request and vehicle fleet data to begin optimization</p>

        {/* Upload Zone */}
        <motion.div
          className={`card border-2 border-dashed mb-6 transition-all ${
            dragActive ? 'border-primary bg-primary/5' : 'border-gray/30'
          }`}
          onDragEnter={handleDrag}
          onDragLeave={handleDrag}
          onDragOver={handleDrag}
          onDrop={handleDrop}
        >
          <div className="text-center py-12">
            <Upload className="w-16 h-16 text-gray mx-auto mb-4" />
            <p className="text-xl mb-2">Drag and drop your Excel file here</p>
            <p className="text-sm text-gray mb-6">Supports .xlsx and .xls formats</p>

            <div className="flex items-center justify-center gap-4 flex-wrap">
              <label className="btn-primary cursor-pointer">
                <input
                  type="file"
                  accept=".xlsx,.xls"
                  onChange={handleFileSelect}
                  className="hidden"
                />
                <FileSpreadsheet className="w-5 h-5 inline mr-2" />
                Browse Files
              </label>

              <button className="btn-secondary">
                <Cloud className="w-5 h-5 inline mr-2" />
                Import from Google Drive
              </button>

              <button className="btn-secondary">
                <Folder className="w-5 h-5 inline mr-2" />
                Import from Dropbox
              </button>
            </div>

            <p className="text-sm text-gray mt-6">
              Or{' '}
              <button className="text-primary hover:underline">try with sample dataset →</button>
            </p>
          </div>
        </motion.div>

        {/* File Confirmation Card */}
        {uploadedFile && (
          <motion.div
            initial={{ opacity: 0, y: 20 }}
            animate={{ opacity: 1, y: 0 }}
            className="card mb-6"
          >
            <div className="flex items-center justify-between">
              <div className="flex items-center gap-4">
                <File className="w-12 h-12 text-primary" />
                <div>
                  <p className="font-medium">{uploadedFile.name}</p>
                  <p className="text-sm text-gray">
                    {(uploadedFile.size / 1024).toFixed(2)} KB
                  </p>
                </div>
                <span className="badge badge-info">Local</span>
              </div>
              <button
                onClick={handleRemove}
                className="text-red-400 hover:text-red-300 transition-colors"
              >
                <X className="w-6 h-6" />
              </button>
            </div>
          </motion.div>
        )}

        {/* Validation Status */}
        {isValidating && (
          <motion.div
            initial={{ opacity: 0 }}
            animate={{ opacity: 1 }}
            className="card mb-6 text-center py-8"
          >
            <div className="animate-spin w-12 h-12 border-4 border-primary border-t-transparent rounded-full mx-auto mb-4" />
            <p className="text-gray">Parsing and validating file...</p>
          </motion.div>
        )}

        {validationError && (
          <motion.div
            initial={{ opacity: 0 }}
            animate={{ opacity: 1 }}
            className="card mb-6 bg-red-500/10 border-red-500/30"
          >
            <div className="flex items-start gap-3">
              <X className="w-6 h-6 text-red-400 flex-shrink-0" />
              <div>
                <p className="font-medium text-red-400 mb-1">Validation Failed</p>
                <p className="text-sm text-gray">{validationError}</p>
              </div>
            </div>
          </motion.div>
        )}

        {parsedData && !validationError && (
          <motion.div
            initial={{ opacity: 0 }}
            animate={{ opacity: 1 }}
            className="card mb-6 bg-green-500/10 border-green-500/30"
          >
            <div className="flex items-start gap-3">
              <Check className="w-6 h-6 text-green-400 flex-shrink-0" />
              <div>
                <p className="font-medium text-green-400 mb-1">File parsed successfully!</p>
                <p className="text-sm text-gray">
                  Found {parsedData.employees.length} employee requests and {parsedData.vehicles.length} vehicles
                </p>
              </div>
            </div>
          </motion.div>
        )}

        {/* Continue Button */}
        <div className="flex justify-end">
          <button
            onClick={handleContinue}
            disabled={!parsedData || !!validationError}
            className="btn-primary disabled:opacity-50 disabled:cursor-not-allowed disabled:hover:scale-100"
          >
            Review Data Insights →
          </button>
        </div>
      </div>
    </div>
  );
}
