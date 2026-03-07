import { useEffect, useRef } from 'react';
import { useNavigate } from 'react-router-dom';
import { motion } from 'framer-motion';
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
          <div className="bg-panel-dark border border-white/10 p-6 text-center">
            {complete ? (
              <>
                <div className="w-16 h-16 rounded-full bg-primary/10 flex items-center justify-center mx-auto mb-4">
                  <span className="material-symbols-outlined text-primary text-4xl">check_circle</span>
                </div>
                <h2 className="text-xl font-black text-white uppercase tracking-tight mb-2">Optimization Complete</h2>
                <p className="text-xs font-mono text-white/30 mb-6">Your last optimization run finished successfully.</p>
                <div className="flex gap-4 justify-center">
                  <button onClick={() => navigate('/results')} className="btn-primary">View Results →</button>
                  <button onClick={() => navigate('/insights')} className="btn-secondary">Re-configure</button>
                </div>
              </>
            ) : (
              <>
                <h2 className="text-xl font-black text-white uppercase tracking-tight mb-2">No Optimization Running</h2>
                <p className="text-xs font-mono text-white/30 mb-6">Ready to run optimization with your current settings.</p>
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
    <div className="h-[calc(100vh-56px)] flex items-center justify-center p-6 network-bg overflow-hidden">
      <motion.div initial={{ opacity: 0, scale: 0.95 }} animate={{ opacity: 1, scale: 1 }} className="w-full max-w-5xl h-full max-h-[600px]">
        <div className="bg-panel-dark border border-white/10 h-full flex flex-col md:flex-row overflow-hidden">
          
          {/* Left Panel — Animation, Progress, Timer */}
          <div className="flex-1 flex flex-col items-center justify-center p-8 border-b md:border-b-0 md:border-r border-white/5">
            {/* Lottie Animation */}
            <div className="w-72 h-72 -mb-16">
              <Lottie 
                animationData={mapSearchAnimation} 
                loop={true}
                autoplay={true}
              />
            </div>

            {/* Percentage */}
            <div className="text-6xl font-black font-mono text-primary mb-2">
              {Math.floor(progress)}%
            </div>

            {/* Stopwatch Timer */}
            <div className="text-3xl font-bold text-white font-mono tracking-widest">
              {formatTime(elapsed)}
            </div>
          </div>

          {/* Right Panel — Stages, Info, Cancel */}
          <div className="flex-1 flex flex-col justify-center p-8 gap-6">
            {/* Stage Indicators */}
            <div>
              <h3 className="text-[11px] font-label font-bold uppercase tracking-widest text-white/40 mb-4">Optimization Stages</h3>
              <div className="space-y-3">
                {OPTIMIZATION_STAGES.map((stage, index) => (
                  <div
                    key={stage}
                    className="flex items-center gap-3 h-6"
                  >
                    {index < currentStage ? (
                      <span className="material-symbols-outlined text-primary text-xl flex-shrink-0 w-5 h-5 leading-5">check_circle</span>
                    ) : index === currentStage ? (
                      <span className="material-symbols-outlined text-primary text-xl flex-shrink-0 w-5 h-5 leading-5 animate-spin">progress_activity</span>
                    ) : (
                      <div className="w-5 h-5 border-2 border-white/10 flex-shrink-0" />
                    )}
                    <span className={`text-xs font-mono ${index <= currentStage ? 'text-white/70' : 'text-white/20'}`}>{stage}</span>
                  </div>
                ))}
              </div>
            </div>

            {/* Cancel button */}
            <button
              onClick={() => {
                cancelOptimization();
                navigate('/insights');
              }}
              className="self-start px-5 py-2 border border-action/30 bg-action/5 text-action text-[11px] font-label font-bold uppercase tracking-widest hover:bg-action/15 hover:border-action/50 transition-all duration-200"
            >
              Cancel Optimization
            </button>
          </div>

        </div>
      </motion.div>
    </div>
  );
}

