import { useLocation } from 'react-router-dom';
import { useApp } from '../context/AppContext';

const steps = [
  {
    id: 1,
    name: 'Upload Data',
    path: '/upload',
    icon: 'upload',
    requires: null as null | 'data' | 'results',
  },
  {
    id: 2,
    name: 'Review Insights & Configure',
    path: '/insights',
    icon: 'settings',
    requires: null as null | 'data' | 'results',
  },
  {
    id: 3,
    name: 'Optimise',
    path: '/processing',
    icon: 'play_arrow',
    requires: null as null | 'data' | 'results',
  },
  {
    id: 4,
    name: 'Results',
    path: '/results',
    icon: 'trending_up',
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
    <div className="border-b border-border-dark py-6 px-8 bg-[rgba(255,255,255,0.02)]">
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
                      ? 'bg-surface-dark border-border-dark'
                      : isCompleted || isActive
                        ? 'bg-primary border-primary'
                        : 'bg-surface-dark border-slate-600'
                  }`}
                >
                  <span 
                    className={`material-symbols-outlined text-xl ${
                      isLocked
                        ? 'text-slate-600'
                        : isCompleted || isActive
                          ? 'text-background-dark'
                          : 'text-slate-500'
                    }`}
                  >
                    {step.icon}
                  </span>
                </div>
                
                {/* Step Label */}
                <p
                  className={`text-sm font-semibold transition-colors duration-300 ${
                    isLocked
                      ? 'text-slate-600'
                      : isActive || isCompleted
                        ? 'text-white'
                        : 'text-slate-500'
                  }`}
                >
                  {step.name}
                </p>
                
                {/* Connector Line */}
                {index < steps.length - 1 && (
                  <div className={`absolute top-6 left-[calc(50%+32px)] right-[calc(-50%+32px)] h-0.5 ${isLocked ? 'bg-border-dark' : 'bg-slate-700'}`}>
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
