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
          DEFAULT: '#13eca0',
          dark: '#0fc488',
          light: '#3dfdb8',
          bright: '#13eca0',
          muted: '#0da87a',
        },
        button: {
          DEFAULT: '#13eca0',
          hover: '#0fc488',
        },
        dark: {
          DEFAULT: '#10221c',
          900: '#0a1511',
          800: '#0d1b15',
          750: '#10221c',
          700: '#142a23',
          600: '#1a3a30',
          500: '#21493d',
        },
        gray: {
          DEFAULT: '#808284',
          light: '#a0a0a0',
          dark: '#606060',
        },
        'background-light': '#f6f8f7',
        'background-dark': '#10221c',
        'surface-dark': 'rgba(255, 255, 255, 0.05)',
        'surface-dark-hover': 'rgba(255, 255, 255, 0.1)',
        'border-dark': 'rgba(255, 255, 255, 0.1)',
      },
      boxShadow: {
        'float': '0 8px 30px rgba(0, 0, 0, 0.4), 0 4px 12px rgba(0, 0, 0, 0.3)',
        'float-lg': '0 20px 60px rgba(0, 0, 0, 0.5), 0 8px 20px rgba(0, 0, 0, 0.4)',
        'float-xl': '0 30px 80px rgba(0, 0, 0, 0.6), 0 12px 30px rgba(0, 0, 0, 0.5)',
        'glow': '0 0 20px rgba(19, 236, 160, 0.3), 0 0 40px rgba(19, 236, 160, 0.1)',
        'glow-lg': '0 0 30px rgba(19, 236, 160, 0.4), 0 0 60px rgba(19, 236, 160, 0.2)',
      },
      fontFamily: {
        sans: ['Inter', 'system-ui', 'sans-serif'],
        display: ['Inter', 'sans-serif'],
      },
      borderRadius: {
        DEFAULT: '0.25rem',
        lg: '0.5rem',
        xl: '0.75rem',
        full: '9999px',
      },
      animation: {
        'pulse-slow': 'pulse 3s cubic-bezier(0.4, 0, 0.6, 1) infinite',
        'float': 'float 6s ease-in-out infinite',
        'gradient': 'gradient 15s ease infinite',
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
      },
      backgroundSize: {
        '300%': '300%',
      },
    },
  },
  plugins: [],
}
