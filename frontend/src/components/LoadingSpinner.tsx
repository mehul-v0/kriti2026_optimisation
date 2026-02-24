import { motion } from 'framer-motion';

interface LoadingSpinnerProps {
  size?: 'sm' | 'md' | 'lg';
  text?: string;
  className?: string;
}

export default function LoadingSpinner({ size = 'md', text, className = '' }: LoadingSpinnerProps) {
  const sizeClasses = {
    sm: 'w-6 h-6',
    md: 'w-8 h-8', 
    lg: 'w-12 h-12'
  };

  const textSizes = {
    sm: 'text-sm',
    md: 'text-base',
    lg: 'text-lg'
  };

  return (
    <motion.div
      initial={{ opacity: 0 }}
      animate={{ opacity: 1 }}
      className={`flex flex-col items-center gap-4 ${className}`}
    >
      {/* Single clean spinner */}
      <div className={`${sizeClasses[size]}`}>
        <div className="animate-spin rounded-full border-4 border-gray-300 border-t-primary w-full h-full"></div>
      </div>

      {/* Text */}
      {text && (
        <div className={`${textSizes[size]} text-gray/80 text-center`}>
          {text}
        </div>
      )}
    </motion.div>
  );
}
