import { useLocation } from 'react-router-dom';
import { Upload, Settings, Play, TrendingUp } from 'lucide-react';
import { useApp } from '../context/AppContext';

const steps = [
  {
    id: 1,
    name: 'Upload Data',
    path: '/upload',
    icon: Upload,
    requires: null as null | 'data' | 'results',
  },
  {
    id: 2,
    name: 'Review Insights & Configure',
    path: '/insights',
    icon: Settings,
    requires: null as null | 'data' | 'results',
  },
  {
    id: 3,
    name: 'Optimise',
    path: '/processing',
    icon: Play,
    requires: null as null | 'data' | 'results',
  },
  {
    id: 4,
    name: 'Results',
    path: '/results',
    icon: TrendingUp,
    requires: null as null | 'data' | 'results',
  },
];

export default function StepProgress() {
  const location = useLocation();
  const { currentResult, optimizationStatus } = useApp();
  
  const showStepProgress = ['/upload', '/insights', '/processing', '/results'].includes(location.pathname);
  
  if (!showStepProgress) {
    return null;
  }

  const hasData = !!sessionStorage.getItem('uploadedData');
  const hasResults = sessionStorage.getItem('optimizationComplete') === 'true' && (optimizationStatus === 'completed' || !!currentResult);

  const isStepLocked = (step: typeof steps[0]) => {
    if (!step.requires) return false;
    if (step.requires === 'data') return !hasData;
    if (step.requires === 'results') return !hasResults;
    return false;
  };

  const getCurrentStep = () => {
    const step = steps.find(s => s.path === location.pathname);
    return step?.id || 1;
  };

  const currentStep = getCurrentStep();
  
  const getStepState = (step: typeof steps[0]) => {
    if (isStepLocked(step)) return 'locked';
    if (step.id === currentStep) return 'active';
    if (step.id < currentStep) return 'completed';
    return 'upcoming';
  };

  return (
    <div className="bg-dark-900/95 border-b border-gray/10 py-6 px-8">
      <div className="max-w-7xl mx-auto">
        <div className="grid grid-cols-4">
          {steps.map((step, index) => {
            const state = getStepState(step);
            const isActive = state === 'active';
            const isCompleted = state === 'completed';
            const isLocked = state === 'locked';
            
            return (
              <div key={step.id} className="flex flex-col items-center text-center relative">
                {/* Step Circle */}
                <div
                  className={`flex items-center justify-center w-12 h-12 rounded-full border-2 transition-all duration-300 mb-3 ${
                    isLocked
                      ? 'bg-dark-800/40 border-gray/10'
                      : isCompleted || isActive
                        ? 'bg-primary border-primary'
                        : 'bg-dark-800 border-gray/30'
                  }`}
                >
                  <step.icon 
                    className={`w-5 h-5 ${
                      isLocked
                        ? 'text-gray/20'
                        : isCompleted || isActive
                          ? 'text-black'
                          : 'text-gray/50'
                    }`} 
                  />
                </div>
                
                {/* Step Label */}
                <p
                  className={`text-sm font-semibold transition-colors duration-300 ${
                    isLocked
                      ? 'text-gray/20'
                      : isActive || isCompleted
                        ? 'text-white'
                        : 'text-gray/50'
                  }`}
                >
                  {step.name}
                </p>
                
                {/* Connector Line */}
                {index < steps.length - 1 && (
                  <div className={`absolute top-6 left-[calc(50%+32px)] right-[calc(-50%+32px)] h-0.5 ${isLocked ? 'bg-gray/10' : 'bg-gray/20'}`}>
                    <div
                      className={`h-full transition-all duration-500 ${
                        isCompleted ? 'bg-primary' : 'bg-transparent'
                      }`}
                    />
                  </div>
                )}
              </div>
            );
          })}
        </div>
      </div>
    </div>
  );
}