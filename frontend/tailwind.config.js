/** @type {import('tailwindcss').Config} */
export default {
  darkMode: 'class',
  content: [
    "./index.html",
    "./src/**/*.{js,ts,jsx,tsx}",
  ],
  theme: {
    extend: {
      colors: {
        primary: {
          DEFAULT: '#FFB800',
          dark: '#E5A600',
          light: '#FFCB40',
          bright: '#FFB800',
          muted: '#C89200',
        },
        action: {
          DEFAULT: '#FF3B30',
          hover: '#FF5549',
        },
        button: {
          DEFAULT: '#FFB800',
          hover: '#FFCB40',
        },
        dark: {
          DEFAULT: '#05070A',
          900: '#05070A',
          800: '#0D1117',
          750: '#10151D',
          700: '#151B26',
          600: '#1C2333',
          500: '#242D40',
        },
        gray: {
          DEFAULT: '#808284',
          light: '#a0a0a0',
          dark: '#606060',
        },
        'background-light': '#f6f8f7',
        'background-dark': '#05070A',
        'panel-dark': '#0D1117',
        'surface-dark': 'rgba(255, 255, 255, 0.03)',
        'surface-dark-hover': 'rgba(255, 255, 255, 0.06)',
        'border-dark': 'rgba(255, 255, 255, 0.08)',
        'terminal-text': '#E2E8F0',
      },
      boxShadow: {
        'float': '0 8px 30px rgba(0, 0, 0, 0.5), 0 4px 12px rgba(0, 0, 0, 0.4)',
        'float-lg': '0 20px 60px rgba(0, 0, 0, 0.6), 0 8px 20px rgba(0, 0, 0, 0.5)',
        'float-xl': '0 30px 80px rgba(0, 0, 0, 0.7), 0 12px 30px rgba(0, 0, 0, 0.6)',
        'glow': '0 0 20px rgba(255, 184, 0, 0.15)',
        'glow-lg': '0 0 30px rgba(255, 184, 0, 0.25)',
        'glow-red': '0 0 20px rgba(255, 59, 48, 0.25)',
      },
      fontFamily: {
        sans: ['Inter', 'system-ui', 'sans-serif'],
        display: ['Inter', 'sans-serif'],
        label: ['Roboto Condensed', 'sans-serif'],
        mono: ['JetBrains Mono', 'monospace'],
      },
      borderRadius: {
        DEFAULT: '0.125rem',
        sm: '0.125rem',
        md: '0.25rem',
        lg: '0.375rem',
        xl: '0.5rem',
        full: '9999px',
      },
      animation: {
        'pulse-slow': 'pulse 3s cubic-bezier(0.4, 0, 0.6, 1) infinite',
        'float': 'float 6s ease-in-out infinite',
        'gradient': 'gradient 15s ease infinite',
        'pulse-border': 'pulse-border 2s cubic-bezier(0.4, 0, 0.6, 1) infinite',
      },
      keyframes: {
        float: {
          '0%, 100%': { transform: 'translateY(0)' },
          '50%': { transform: 'translateY(-20px)' },
        },
        gradient: {
          '0%, 100%': { backgroundPosition: '0% 50%' },
          '50%': { backgroundPosition: '100% 50%' },
        },
        'pulse-border': {
          '0%, 100%': { borderColor: 'rgba(255, 184, 0, 0.2)' },
          '50%': { borderColor: 'rgba(255, 184, 0, 0.6)' },
        },
      },
      backgroundSize: {
        '300%': '300%',
      },
    },
  },
  plugins: [],
}
