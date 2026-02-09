import { useEffect, useRef } from 'react';
import { useNavigate } from 'react-router-dom';
import { motion } from 'framer-motion';
import { Check, Loader2 } from 'lucide-react';
import { useApp, OPTIMIZATION_STAGES } from '../context/AppContext';

export default function OptimizationProcessing() {
  const navigate = useNavigate();
  const {
    optimizationStatus,
    optimizationProgress,
    optimizationStage,
    cancelOptimization,
    solverDuration,
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
  const elapsed = Math.floor((progress / 100) * solverDuration);
  const remaining = solverDuration - elapsed;

  if (optimizationStatus !== 'running') {
    // Show a static page when not currently running
    const complete = sessionStorage.getItem('optimizationComplete') === 'true';
    return (
      <div className="min-h-screen bg-dark flex items-center justify-center p-8 network-bg">
        <motion.div initial={{ opacity: 0, scale: 0.9 }} animate={{ opacity: 1, scale: 1 }} className="max-w-lg w-full">
          <div className="bg-dark-800/60 backdrop-blur-xl rounded-2xl border border-gray/10 shadow-2xl shadow-black/40 p-6 text-center py-16">
            {complete ? (
              <>
                <div className="w-16 h-16 rounded-full bg-green-500/20 flex items-center justify-center mx-auto mb-6">
                  <Check className="w-8 h-8 text-green-400" />
                </div>
                <h2 className="text-2xl font-bold mb-3">Optimization Complete</h2>
                <p className="text-gray mb-6">Your last optimization run finished successfully.</p>
                <div className="flex gap-4 justify-center">
                  <button onClick={() => navigate('/results')} className="btn-primary">View Results →</button>
                  <button onClick={() => navigate('/insights')} className="btn-secondary">Re-configure</button>
                </div>
              </>
            ) : (
              <>
                <div className="w-16 h-16 rounded-full bg-dark-600 flex items-center justify-center mx-auto mb-6">
                  <Loader2 className="w-8 h-8 text-gray/50" />
                </div>
                <h2 className="text-2xl font-bold mb-3">No Optimization Running</h2>
                <p className="text-gray mb-6">Configure your settings and start an optimization run.</p>
                <button onClick={() => navigate('/insights')} className="btn-primary">Go to Settings →</button>
              </>
            )}
          </div>
        </motion.div>
      </div>
    );
  }

  return (
    <div className="min-h-screen bg-dark flex items-center justify-center p-8 network-bg">
      <motion.div initial={{ opacity: 0, scale: 0.9 }} animate={{ opacity: 1, scale: 1 }} className="max-w-2xl w-full">
        {/* Central Progress Circle */}
        <div className="bg-dark-800/60 backdrop-blur-xl rounded-2xl border border-gray/10 shadow-2xl shadow-black/40 p-6 text-center py-16">
          <div className="relative w-48 h-48 mx-auto mb-8">
            <svg className="w-full h-full transform -rotate-90">
              <circle cx="96" cy="96" r="88" stroke="currentColor" strokeWidth="8" fill="none" className="text-dark-600" />
              <circle
                cx="96"
                cy="96"
                r="88"
                stroke="currentColor"
                strokeWidth="8"
                fill="none"
                className="text-primary transition-all"
                strokeDasharray={`${2 * Math.PI * 88}`}
                strokeDashoffset={`${2 * Math.PI * 88 * (1 - progress / 100)}`}
                strokeLinecap="round"
              />
            </svg>
            <div className="absolute inset-0 flex flex-col items-center justify-center">
              <div className="text-4xl font-bold text-primary">{Math.floor(progress)}%</div>
              <div className="text-sm text-gray mt-2">
                {Math.floor(elapsed / 60)}:{(elapsed % 60).toString().padStart(2, '0')} / {Math.floor(solverDuration / 60)}:{(solverDuration % 60).toString().padStart(2, '0')}
              </div>
            </div>
          </div>

          {/* Stage Indicators */}
          <div className="space-y-3 max-w-md mx-auto">
            {OPTIMIZATION_STAGES.map((stage, index) => (
              <motion.div
                key={stage}
                initial={{ opacity: 0, x: -20 }}
                animate={{ opacity: 1, x: 0 }}
                transition={{ delay: index * 0.1 }}
                className="flex items-center gap-3"
              >
                {index < currentStage ? (
                  <Check className="w-5 h-5 text-green-400 flex-shrink-0" />
                ) : index === currentStage ? (
                  <Loader2 className="w-5 h-5 text-primary animate-spin flex-shrink-0" />
                ) : (
                  <div className="w-5 h-5 rounded-full border-2 border-gray/30 flex-shrink-0" />
                )}
                <span className={index <= currentStage ? 'text-white' : 'text-gray'}>{stage}</span>
              </motion.div>
            ))}
          </div>

          <div className="mt-8 text-gray">
            <p>Optimizing routes for {JSON.parse(sessionStorage.getItem('uploadedData') || '{}').employees?.length || 0} employees</p>
            <p className="text-sm mt-2">
              Estimated completion: {new Date(Date.now() + remaining * 1000).toLocaleTimeString()}
            </p>
          </div>

          <button
            onClick={() => {
              cancelOptimization();
              navigate('/insights');
            }}
            className="mt-8 text-sm text-gray hover:text-white transition-colors"
          >
            Cancel optimization
          </button>
        </div>
      </motion.div>
    </div>
  );
}
