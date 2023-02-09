pipeline {
  agent {
    node {
      label 'Linux'
    }

  }
  stages {
    stage('Checkout') {
      steps {
        git(url: 'https://github.com/rohmer/LVGL_UI_Creator', branch: 'Dev', poll: true)
      }
    }
    stage('error') {
      steps {
        cmake(installation: 'LinuxCMake', workingDir: 'built')
      }
    }
  }
}