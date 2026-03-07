import { motion } from 'framer-motion';

interface LoadingSpinnerProps {
  size?: 'sm' | 'md' | 'lg';
  text?: string;
  className?: string;
}

export default function LoadingSpinner({ size = 'md', text, className = '' }: LoadingSpinnerProps) {
  const sizeClasses = {
    sm: 'w-5 h-5',
    md: 'w-7 h-7',
    lg: 'w-10 h-10'
  };

  const borderWidth = { sm: '2px', md: '2px', lg: '3px' };

  return (
    <motion.div
      initial={{ opacity: 0 }}
      animate={{ opacity: 1 }}
      className={`flex items-center gap-3 ${className}`}
    >
      {/* Terminal-style spinner */}
      <div
        className={`${sizeClasses[size]} animate-spin`}
        style={{
          border: `${borderWidth[size]} solid rgba(255,255,255,0.06)`,
          borderTop: `${borderWidth[size]} solid #FFB800`,
        }}
      />

      {/* Monospace label */}
      {text && (
        <div className="flex items-center gap-1.5">
          <span className="text-[10px] font-mono uppercase tracking-widest text-white/40">
            {text}
          </span>
          <motion.span
            animate={{ opacity: [1, 0.2, 1] }}
            transition={{ duration: 1.2, repeat: Infinity, ease: 'easeInOut' }}
            className="text-primary font-mono text-xs"
          >
            ...
          </motion.span>
        </div>
      )}
    </motion.div>
  );
}
