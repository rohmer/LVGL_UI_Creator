pipeline {
  agent {
    node {
      label 'PILinux'
    }

  }
  stages {
    stage('Build') {
      steps {
        git(url: 'https://github.com/rohmer/LVGL_UI_Creator', branch: 'Dev', poll: true)
        cmake 'CMake'
      }
    }
  }
}