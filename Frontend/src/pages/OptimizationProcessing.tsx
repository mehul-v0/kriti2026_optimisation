import { useEffect, useRef } from 'react';
import { useNavigate } from 'react-router-dom';
import { motion } from 'framer-motion';
import { Check, Loader2 } from 'lucide-react';
import Lottie from 'lottie-react';
import { useApp, OPTIMIZATION_STAGES } from '../context/AppContext';
import mapSearchAnimation from '../assets/map search.json';

export default function OptimizationProcessing() {
  const navigate = useNavigate();
  const {
    optimizationStatus,
    optimizationProgress,
    optimizationStage,
    cancelOptimization,
    solverDuration,
    startOptimization,
  } = useApp();

  // Track whether we were running when we mounted / last rendered
  const wasRunningRef = useRef(optimizationStatus === 'running');

  // If idle and no data, redirect to insights
  useEffect(() => {
    if (optimizationStatus === 'idle' || optimizationStatus === 'error') {
      const hasData = !!sessionStorage.getItem('uploadedData');
      if (!hasData) {
        navigate('/insights');
      }
    }
  }, [optimizationStatus, navigate]);

  // Only auto-navigate to results when transitioning from running → completed
  useEffect(() => {
    if (wasRunningRef.current && optimizationStatus === 'completed') {
      navigate('/results');
    }
    wasRunningRef.current = optimizationStatus === 'running';
  }, [optimizationStatus, navigate]);

  const progress = optimizationProgress;
  const currentStage = optimizationStage;
  // Calculate elapsed time with decimal precision for stopwatch display
  const elapsed = progress >= 100 ? solverDuration : (progress / 100) * solverDuration;

  // Format time as MM:SS:HH (minutes:seconds:hundredths)
  const formatTime = (timeInSeconds: number) => {
    const minutes = Math.floor(timeInSeconds / 60);
    const seconds = Math.floor(timeInSeconds % 60);
    const hundredths = Math.floor((timeInSeconds % 1) * 100);
    return `${String(minutes).padStart(2, '0')}:${String(seconds).padStart(2, '0')}:${String(hundredths).padStart(2, '0')}`;
  };

  if (optimizationStatus !== 'running') {
    // Show a static page when not currently running
    const complete = sessionStorage.getItem('optimizationComplete') === 'true';
    return (
      <div className="min-h-screen bg-dark flex items-center justify-center p-4 network-bg">
        <motion.div initial={{ opacity: 0, scale: 0.9 }} animate={{ opacity: 1, scale: 1 }} className="max-w-lg w-full">
          <div className="bg-dark-800/60 backdrop-blur-xl rounded-2xl border border-gray/10 p-6 text-center shadow-float">
            {complete ? (
              <>
                <div className="w-16 h-16 rounded-full bg-primary/20 flex items-center justify-center mx-auto mb-4">
                  <Check className="w-8 h-8 text-primary-bright" />
                </div>
                <h2 className="text-2xl font-bold mb-2">Optimization Complete</h2>
                <p className="text-gray mb-6">Your last optimization run finished successfully.</p>
                <div className="flex gap-4 justify-center">
                  <button onClick={() => navigate('/results')} className="btn-primary">View Results →</button>
                  <button onClick={() => navigate('/insights')} className="btn-secondary">Re-configure</button>
                </div>
              </>
            ) : (
              <>
                <h2 className="text-2xl font-bold mb-2">No Optimization Running</h2>
                <p className="text-gray mb-6">Ready to run optimization with your current settings.</p>
                <div className="flex gap-4 justify-center">
                  <button 
                    onClick={() => {
                      const hasData = !!sessionStorage.getItem('uploadedData');
                      if (hasData) {
                        startOptimization();
                      } else {
                        navigate('/upload');
                      }
                    }} 
                    className="btn-primary"
                  >
                    Run Optimization →
                  </button>
                  <button onClick={() => navigate('/insights')} className="btn-secondary">Configure Settings</button>
                </div>
              </>
            )}
          </div>
        </motion.div>
      </div>
    );
  }

  return (
    <div className="min-h-screen bg-dark flex items-center justify-center p-4 network-bg">
      <motion.div initial={{ opacity: 0, scale: 0.9 }} animate={{ opacity: 1, scale: 1 }} className="max-w-2xl w-full">
        {/* Central Progress Circle */}
        <div className="bg-dark-800/60 backdrop-blur-xl rounded-2xl border border-gray/10 p-8 text-center">
          {/* Lottie Animation */}
          <div className="flex justify-center -mb-40">
            <div className="w-96 h-96">
              <Lottie 
                animationData={mapSearchAnimation} 
                loop={true}
                autoplay={true}
              />
            </div>
          </div>

          {/* Progress and Timer Display */}
          <div className="mb-8 space-y-4">
            {/* Percentage */}
            <div className="text-5xl font-bold text-primary">
              {Math.floor(progress)}%
            </div>
            {/* Stopwatch Timer */}
            <div className="text-4xl font-bold text-white font-mono tracking-widest">
              {formatTime(elapsed)}
            </div>
          </div>

          {/* Stage Indicators */}
          <div className="space-y-2 max-w-md mx-auto mb-6">
            {OPTIMIZATION_STAGES.map((stage, index) => (
              <motion.div
                key={stage}
                initial={{ opacity: 0, x: -20 }}
                animate={{ opacity: 1, x: 0 }}
                transition={{ delay: index * 0.1 }}
                className="flex items-center gap-3"
              >
                {index < currentStage ? (
                  <Check className="w-5 h-5 text-primary-bright flex-shrink-0" />
                ) : index === currentStage ? (
                  <Loader2 className="w-5 h-5 text-primary animate-spin flex-shrink-0" />
                ) : (
                  <div className="w-5 h-5 rounded-full border-2 border-gray/30 flex-shrink-0" />
                )}
                <span className={index <= currentStage ? 'text-white' : 'text-gray'}>{stage}</span>
              </motion.div>
            ))}
          </div>

          <div className="text-gray mb-4">
            <p className="font-medium">Optimizing routes for {JSON.parse(sessionStorage.getItem('uploadedData') || '{}').employees?.length || 0} employees</p>
            {progress >= 100 && (
              <p className="text-sm mt-2 text-primary-bright">
                Processing complete - Retrieving results...
              </p>
            )}
          </div>

          <button
            onClick={() => {
              cancelOptimization();
              navigate('/insights');
            }}
            className="text-sm text-gray hover:text-white transition-colors"
          >
            Cancel optimization
          </button>
        </div>
      </motion.div>
    </div>
  );
}

