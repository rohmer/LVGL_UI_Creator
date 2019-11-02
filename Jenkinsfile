pipeline {
  agent {
    node {
      label 'PILinux'
    }

  }
  stages {
    stage('Checkout') {
      steps {
        git(url: 'https://github.com/rohmer/LVGL_UI_Creator', branch: 'Dev', poll: true)
      }
    }
    stage('') {
      steps {
        cmake(installation: 'LinuxCMake', workingDir: 'built')
      }
    }
  }
}